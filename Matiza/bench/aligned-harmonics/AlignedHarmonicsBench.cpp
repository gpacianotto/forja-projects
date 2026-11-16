// Benchmark do estudo de harmônicos alinhados à escala (card "DSP: harmônicos alinhados à escala").
//
// Três perguntas:
//  1. CPU: quanto custa um motor aditivo (uma senoide por harmônico) contra o wavetable atual,
//     no pior caso do Matiza (192 osciladores = 8 vozes x 3 osc x 8 cópias de unison)?
//  2. Som: o alinhamento reduz a dissonância (aspereza/batimento) dos acordes? Medido com o modelo
//     de Plomp-Levelt na forma usada por William Sethares ("Tuning, Timbre, Spectrum, Scale").
//  3. Escuta: gera .wav de acordes com e sem alinhamento, para comparar de ouvido.
#include "Oscillator.h"
#include "PartialBank.h"

#include <immintrin.h> // SSE (4 floats por instrução); todo processador x64 tem

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <string>
#include <vector>

constexpr double sampleRate = 48000.0;
constexpr double twoPi = 2.0 * std::numbers::pi;
constexpr int blockSize = 64;

// =====================================================================================
// Alinhamento (lógica pura; seria a base de um AlignedSpectrum.h no plugin)
// =====================================================================================

// Afinação vista como lista de alturas absolutas (em cents a partir de uma nota qualquer), como o
// TuningEngine já sabe produzir para EDO, afinação desenhada (RF17) e master MTS-ESP (RF18).
// Para o estudo, uma EDO: degrau = 1200 / edo.
struct EdoScale
{
    int edo;
    double step() const { return 1200.0 / edo; }
    double nearest (double cents) const { return std::round (cents / step()) * step(); }
};

// Posição (razão de frequência) do harmônico h depois do alinhamento.
// align = 0: harmônico natural (h); align = 1: no grau da escala mais próximo. Entre os dois,
// interpola em cents (o knob "Align" sugerido no card).
double alignedRatio (int h, const EdoScale& scale, double align)
{
    const double natural = 1200.0 * std::log2 (static_cast<double> (h));
    const double target = scale.nearest (natural);
    return std::exp2 ((natural + align * (target - natural)) / 1200.0);
}

// Quantos harmônicos dá para alinhar antes de dois caírem no mesmo grau da escala.
// Harmônicos vizinhos h e h+1 ficam a 1200*log2((h+1)/h) cents; quando isso fica menor que o
// degrau, eles começam a colidir. Acima desse ponto os harmônicos já são mais densos que a escala.
int collisionLimit (const EdoScale& scale)
{
    for (int h = 1; h < 1000; ++h)
        if (scale.nearest (1200.0 * std::log2 (h + 1.0)) == scale.nearest (1200.0 * std::log2 (h + 0.0)))
            return h;
    return 1000;
}

// =====================================================================================
// Dissonância sensorial (Plomp-Levelt / Sethares)
// =====================================================================================
struct Partial { double hz, amp; };

double pairDissonance (Partial a, Partial b)
{
    if (b.hz < a.hz) std::swap (a, b);
    const double s = 0.24 / (0.0207 * a.hz + 18.96); // a curva se alarga nas regiões graves
    const double d = b.hz - a.hz;
    return std::min (a.amp, b.amp) * (std::exp (-3.51 * s * d) - std::exp (-5.75 * s * d));
}

double totalDissonance (const std::vector<Partial>& partials)
{
    double sum = 0.0;
    for (size_t i = 0; i < partials.size(); ++i)
        for (size_t j = i + 1; j < partials.size(); ++j)
            sum += pairDissonance (partials[i], partials[j]);
    return sum;
}

// Serra: amplitude 1/h. numHarmonics primeiros harmônicos de cada nota do acorde.
std::vector<Partial> chordSpectrum (double baseHz, const std::vector<double>& noteCents,
                                    const EdoScale& scale, double align, int numHarmonics)
{
    std::vector<Partial> out;
    for (double c : noteCents)
    {
        const double f0 = baseHz * std::exp2 (c / 1200.0);
        for (int h = 1; h <= numHarmonics; ++h)
            out.push_back ({ f0 * alignedRatio (h, scale, align), 1.0 / h });
    }
    return out;
}

// =====================================================================================
// Motores para medir CPU
// =====================================================================================

// (a) Aditivo ingênuo: um acumulador de fase + tabela de seno por harmônico. Custa o mesmo que
// um oscilador wavetable POR HARMÔNICO.
struct AdditiveTable
{
    static constexpr int maxPartials = 64;
    int n = 0;
    std::array<double, maxPartials> phase {}, inc {};
    std::array<float, maxPartials> amp {};
    const float* sine = Wavetables::table (Waveform::sine, 0);

    void setFrequency (double hz, const std::vector<double>& ratios)
    {
        for (int k = 0; k < n; ++k) inc[k] = hz * ratios[k] / sampleRate;
    }

    float next()
    {
        float out = 0.0f;
        for (int k = 0; k < n; ++k)
        {
            const double pos = phase[k] * Wavetables::tableSize;
            const int i = static_cast<int> (pos);
            const float f = static_cast<float> (pos - i);
            out += amp[k] * (sine[i] + f * (sine[i + 1] - sine[i]));
            phase[k] += inc[k];
            if (phase[k] >= 1.0) phase[k] -= 1.0;
        }
        return out;
    }
};

// (b) Aditivo por recorrência: cada harmônico é um vetor (cos, sen) que gira um ângulo fixo a
// cada amostra. Girar = 4 multiplicações e 2 somas, sem seno nem tabela. Os dados ficam em
// arrays separados (structure of arrays) para a CPU processar 4 harmônicos de uma vez (SSE).
struct AdditiveRecurrence
{
    static constexpr int maxPartials = 64;
    int n = 0; // múltiplo de 4
    alignas (16) float c[maxPartials] {}, s[maxPartials] {}, rc[maxPartials] {}, rs[maxPartials] {}, amp[maxPartials] {};

    // Recalcula o giro de cada harmônico (cos e sen do passo de fase). Chamado uma vez por bloco
    // quando a frequência muda (pitch bend, vibrato, glide): é a parte cara, n senos por bloco.
    void setFrequency (double hz, const std::vector<double>& ratios)
    {
        for (int k = 0; k < n; ++k)
        {
            const double w = twoPi * hz * ratios[k] / sampleRate;
            rc[k] = static_cast<float> (std::cos (w));
            rs[k] = static_cast<float> (std::sin (w));
        }
    }

    void start()
    {
        for (int k = 0; k < n; ++k) { c[k] = 1.0f; s[k] = 0.0f; }
    }

    // Em float, o vetor perde ou ganha tamanho aos poucos (erro de arredondamento). Uma correção
    // por bloco (aproximação de 1/sqrt(c²+s²) perto de 1) mantém o volume estável.
    void renormalise()
    {
        for (int k = 0; k < n; ++k)
        {
            const float g = 1.5f - 0.5f * (c[k] * c[k] + s[k] * s[k]);
            c[k] *= g;
            s[k] *= g;
        }
    }

    float nextScalar()
    {
        float out = 0.0f;
        for (int k = 0; k < n; ++k)
        {
            const float nc = c[k] * rc[k] - s[k] * rs[k];
            const float ns = c[k] * rs[k] + s[k] * rc[k];
            c[k] = nc;
            s[k] = ns;
            out += amp[k] * ns;
        }
        return out;
    }

    float nextSse()
    {
        __m128 acc = _mm_setzero_ps();
        for (int k = 0; k < n; k += 4)
        {
            const __m128 C = _mm_load_ps (c + k), S = _mm_load_ps (s + k);
            const __m128 RC = _mm_load_ps (rc + k), RS = _mm_load_ps (rs + k);
            const __m128 nc = _mm_sub_ps (_mm_mul_ps (C, RC), _mm_mul_ps (S, RS));
            const __m128 ns = _mm_add_ps (_mm_mul_ps (C, RS), _mm_mul_ps (S, RC));
            _mm_store_ps (c + k, nc);
            _mm_store_ps (s + k, ns);
            acc = _mm_add_ps (acc, _mm_mul_ps (ns, _mm_load_ps (amp + k)));
        }
        // soma horizontal dos 4 floats
        __m128 shuf = _mm_shuffle_ps (acc, acc, _MM_SHUFFLE (2, 3, 0, 1));
        __m128 sums = _mm_add_ps (acc, shuf);
        shuf = _mm_movehl_ps (shuf, sums);
        return _mm_cvtss_f32 (_mm_add_ss (sums, shuf));
    }
};

template <typename Fn>
double timeIt (Fn fn)
{
    const auto t0 = std::chrono::high_resolution_clock::now();
    fn();
    const auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double> (t1 - t0).count();
}

constexpr int numOsc = 192;
const int numSamples = static_cast<int> (sampleRate); // 1 s de áudio
volatile float sink = 0.0f;

// Roda 1 s de áudio em blocos de 64, oscilador por oscilador (como a voz faz), e devolve a
// porcentagem de um núcleo. perBlock: chamado no começo de cada bloco (ex.: mudar a frequência).
template <typename Osc, typename Next, typename PerBlock>
double runBench (std::vector<Osc>& oscs, Next next, PerBlock perBlock)
{
    std::array<float, blockSize> out {};
    const double secs = timeIt ([&] {
        float acc = 0.0f;
        for (int b = 0; b < numSamples / blockSize; ++b)
        {
            out.fill (0.0f);
            for (int o = 0; o < numOsc; ++o)
            {
                perBlock (oscs[o], o, b);
                for (int i = 0; i < blockSize; ++i)
                    out[i] += next (oscs[o]);
            }
            acc += out[0];
        }
        sink = acc;
    });
    return 100.0 * secs * sampleRate / numSamples;
}

void cpuBenchmarks()
{
    const EdoScale edo19 { 19 };
    std::vector<double> ratios (64);
    for (int h = 1; h <= 64; ++h) ratios[h - 1] = alignedRatio (h, edo19, 1.0);
    auto hzOf = [] (int o) { return 110.0 + o; };
    auto noop = [] (auto&, int, int) {};

    std::printf ("CPU: %d osciladores (8 vozes x 3 osc x 8 de unison), 1 s a 48 kHz, %% de um nucleo\n", numOsc);

    {
        std::vector<Oscillator> oscs (numOsc);
        for (int o = 0; o < numOsc; ++o) { oscs[o].setWaveform (Waveform::saw); oscs[o].setFrequency (hzOf (o), sampleRate); }
        std::printf ("  Wavetable atual (RF13)                         : %6.1f %%\n",
                     runBench (oscs, [] (Oscillator& x) { return x.nextSample(); }, noop));
    }

    for (int n : { 16, 32, 64 })
    {
        std::printf ("  -- %d harmonicos --\n", n);
        {
            std::vector<AdditiveTable> oscs (numOsc);
            for (int o = 0; o < numOsc; ++o)
            {
                oscs[o].n = n;
                for (int k = 0; k < n; ++k) oscs[o].amp[k] = 1.0f / (k + 1);
                oscs[o].setFrequency (hzOf (o), ratios);
            }
            std::printf ("  (a) tabela de seno por harmonico              : %6.1f %%\n",
                         runBench (oscs, [] (AdditiveTable& x) { return x.next(); }, noop));
        }

        auto makeRec = [&] {
            std::vector<AdditiveRecurrence> oscs (numOsc);
            for (int o = 0; o < numOsc; ++o)
            {
                oscs[o].n = n;
                for (int k = 0; k < n; ++k) oscs[o].amp[k] = 1.0f / (k + 1);
                oscs[o].start();
                oscs[o].setFrequency (hzOf (o), ratios);
            }
            return oscs;
        };
        auto renorm = [] (AdditiveRecurrence& x, int, int) { x.renormalise(); };
        // Frequência mudando a cada bloco (vibrato): recalcula os giros com cos/sin.
        auto vibrato = [&] (AdditiveRecurrence& x, int o, int b) {
            x.renormalise();
            x.setFrequency (hzOf (o) * (1.0 + 0.01 * std::sin (0.05 * b)), ratios);
        };

        auto rec1 = makeRec();
        std::printf ("  (b) recorrencia, escalar                      : %6.1f %%\n",
                     runBench (rec1, [] (AdditiveRecurrence& x) { return x.nextScalar(); }, renorm));
        auto rec2 = makeRec();
        std::printf ("  (c) recorrencia, SSE                          : %6.1f %%\n",
                     runBench (rec2, [] (AdditiveRecurrence& x) { return x.nextSse(); }, renorm));
        auto rec3 = makeRec();
        std::printf ("  (d) recorrencia, SSE, frequencia mudando      : %6.1f %%\n",
                     runBench (rec3, [] (AdditiveRecurrence& x) { return x.nextSse(); }, vibrato));
    }

    // (e) Híbrido: os K primeiros harmônicos no aditivo (alinhados), o resto no wavetable (uma
    // tabela sem os K primeiros). O custo da tabela não depende do conteúdo dela.
    for (int k : { 16, 24, 32 })
    {
        struct Hybrid { Oscillator wt; AdditiveRecurrence add; };
        std::vector<Hybrid> oscs (numOsc);
        for (int o = 0; o < numOsc; ++o)
        {
            oscs[o].wt.setWaveform (Waveform::saw);
            oscs[o].wt.setFrequency (hzOf (o), sampleRate);
            oscs[o].add.n = k;
            for (int i = 0; i < k; ++i) oscs[o].add.amp[i] = 1.0f / (i + 1);
            oscs[o].add.start();
            oscs[o].add.setFrequency (hzOf (o), ratios);
        }
        std::printf ("  (e) hibrido: %2d no aditivo SSE + wavetable    : %6.1f %%\n", k,
                     runBench (oscs, [] (Hybrid& x) { return x.wt.nextSample() + x.add.nextSse(); },
                               [] (Hybrid& x, int, int) { x.add.renormalise(); }));
    }

    // (f) Classes reais do plugin (RF21): Oscillator lendo o resto + PartialBank, com a
    // renormalização a cada 32 amostras, como na voz.
    {
        struct Copy { Oscillator rest; PartialBank bank; };
        std::vector<Copy> oscs (numOsc);
        AlignedSpectrum::Ratios r;
        for (int h = 1; h <= PartialBank::size; ++h) r.ratio[static_cast<size_t> (h - 1)] = ratios[static_cast<size_t> (h - 1)];
        for (int o = 0; o < numOsc; ++o)
        {
            oscs[o].rest.setWaveform (Waveform::saw);
            oscs[o].rest.setResidual (true);
            oscs[o].rest.setFrequency (hzOf (o), sampleRate);
            oscs[o].bank.setRecipe (AlignedSpectrum::builtinRecipe (Waveform::saw));
            oscs[o].bank.start (0.0);
            oscs[o].bank.setFrequency (hzOf (o), r, sampleRate);
        }
        std::printf ("  (f) Matiza: Oscillator (resto) + PartialBank  : %6.1f %%\n",
                     runBench (oscs, [] (Copy& x) { return x.rest.nextSample() + x.bank.nextSample(); },
                               [] (Copy& x, int, int) { x.bank.renormalise(); }));
        std::printf ("  (f) idem, frequencia mudando a cada bloco     : %6.1f %%\n",
                     runBench (oscs, [] (Copy& x) { return x.rest.nextSample() + x.bank.nextSample(); },
                               [&] (Copy& x, int o, int b) {
                                   x.bank.renormalise();
                                   x.bank.setFrequency (hzOf (o) * (1.0 + 0.01 * std::sin (0.05 * b)), r, sampleRate);
                               }));
    }

    // Precisão da recorrência em float: compara 10 s com seno em double.
    {
        AdditiveRecurrence r;
        r.n = 4;
        r.amp[0] = 1.0f;
        r.start();
        const double hz = 440.0 * ratios[0];
        r.setFrequency (440.0, ratios);
        double maxErr = 0.0;
        for (int i = 0; i < 10 * static_cast<int> (sampleRate); ++i)
        {
            if (i % blockSize == 0) r.renormalise();
            const float v = r.nextSse();
            maxErr = std::max (maxErr, std::abs (v - std::sin (twoPi * hz * (i + 1) / sampleRate)));
        }
        std::printf ("\nErro maximo da recorrencia em float apos 10 s (440 Hz): %.2e (%.0f dB)\n",
                     maxErr, 20.0 * std::log10 (maxErr));
    }
}

// =====================================================================================
// Dissonância de acordes
// =====================================================================================
void dissonanceTables()
{
    constexpr double base = 261.63; // dó central
    constexpr int numHarm = 12;

    struct Case { const char* name; int edo; std::vector<int> steps; };
    const std::vector<Case> cases {
        { "12 EDO maior (0-4-7)", 12, { 0, 4, 7 } },
        { "12 EDO menor (0-3-7)", 12, { 0, 3, 7 } },
        { "19 EDO maior (0-6-11)", 19, { 0, 6, 11 } },
        { "19 EDO menor (0-5-11)", 19, { 0, 5, 11 } },
        { "31 EDO maior (0-10-18)", 31, { 0, 10, 18 } },
        { "10 EDO (0-3-6)", 10, { 0, 3, 6 } },
        { "7 EDO (0-2-4)", 7, { 0, 2, 4 } },
        { "5 EDO (0-1-3)", 5, { 0, 1, 3 } },
        { "13 EDO (0-4-8)", 13, { 0, 4, 8 } },
    };

    std::printf ("\nDissonancia (Plomp-Levelt/Sethares), serra com %d harmonicos, base %.2f Hz.\n", numHarm, base);
    std::printf ("Referencia: 12 EDO maior com harmonicos naturais. Quanto menor, menos aspero.\n");
    const EdoScale edo12 { 12 };
    const double ref = totalDissonance (chordSpectrum (base, { 0.0, 400.0, 700.0 }, edo12, 0.0, numHarm));
    std::printf ("  %-24s | natural | align 50%% | align 100%% | limite de colisao\n", "acorde");
    for (const auto& c : cases)
    {
        const EdoScale scale { c.edo };
        std::vector<double> cents;
        for (int s : c.steps) cents.push_back (s * scale.step());
        const double d0 = totalDissonance (chordSpectrum (base, cents, scale, 0.0, numHarm));
        const double d5 = totalDissonance (chordSpectrum (base, cents, scale, 0.5, numHarm));
        const double d1 = totalDissonance (chordSpectrum (base, cents, scale, 1.0, numHarm));
        std::printf ("  %-24s |  %5.2f  |   %5.2f   |   %5.2f    | %d harmonicos\n",
                     c.name, d0 / ref, d5 / ref, d1 / ref, collisionLimit (scale));
    }

    std::printf ("\nHarmonicos alinhados em 19 EDO (razao natural -> alinhada, desvio em cents)\n");
    const EdoScale edo19 { 19 };
    for (int h = 1; h <= 16; ++h)
    {
        const double a = alignedRatio (h, edo19, 1.0);
        std::printf ("  h=%2d: %6.3f -> %6.3f (%+5.1f ct)\n", h, static_cast<double> (h), a,
                     1200.0 * std::log2 (a / h));
    }

    std::printf ("\nLimite de colisao por EDO (harmonicos alinhaveis sem dois no mesmo grau)\n ");
    for (int e : { 5, 7, 10, 12, 13, 17, 19, 22, 24, 31, 41, 53, 72 })
        std::printf (" %d EDO: %d |", e, collisionLimit ({ e }));
    std::printf ("\n");
}

// =====================================================================================
// Áudios para escuta
// =====================================================================================
void writeWav (const std::filesystem::path& path, const std::vector<float>& samples)
{
    std::ofstream f (path, std::ios::binary);
    auto u32 = [&] (uint32_t v) { f.write (reinterpret_cast<const char*> (&v), 4); };
    auto u16 = [&] (uint16_t v) { f.write (reinterpret_cast<const char*> (&v), 2); };
    const uint32_t dataBytes = static_cast<uint32_t> (samples.size() * 2);
    f.write ("RIFF", 4); u32 (36 + dataBytes); f.write ("WAVE", 4);
    f.write ("fmt ", 4); u32 (16); u16 (1); u16 (1); u32 (48000); u32 (96000); u16 (2); u16 (16);
    f.write ("data", 4); u32 (dataBytes);
    for (float s : samples)
    {
        const auto v = static_cast<int16_t> (std::clamp (s, -1.0f, 1.0f) * 32767.0f);
        f.write (reinterpret_cast<const char*> (&v), 2);
    }
}

// Acorde de serra aditiva (harmônicos até Nyquist ou o limite de colisão; os de cima ficam
// naturais, como no híbrido), 4 s, ataque e release suaves.
std::vector<float> renderChord (const std::vector<double>& cents, const EdoScale& scale, double align)
{
    constexpr double base = 220.0;
    constexpr int maxHarm = 40;
    const int alignedUpTo = collisionLimit (scale);
    const int n = static_cast<int> (4.0 * sampleRate);
    std::vector<float> out (n);

    for (double c : cents)
    {
        const double f0 = base * std::exp2 (c / 1200.0);
        for (int h = 1; h <= maxHarm; ++h)
        {
            const double ratio = h <= alignedUpTo ? alignedRatio (h, scale, align) : h;
            const double hz = f0 * ratio;
            if (hz >= sampleRate * 0.45) break;
            const double w = twoPi * hz / sampleRate;
            for (int i = 0; i < n; ++i)
                out[i] += static_cast<float> (std::sin (w * i) / h);
        }
    }

    for (int i = 0; i < n; ++i)
    {
        const double t = i / sampleRate;
        const double env = std::min ({ 1.0, t / 0.05, (4.0 - t) / 0.5 });
        out[i] *= static_cast<float> (0.12 * env);
    }
    return out;
}

void renderAudio()
{
    const std::filesystem::path dir = AUDIO_DIR;
    std::filesystem::create_directories (dir);

    struct Item { const char* file; int edo; std::vector<int> steps; double align; };
    const std::vector<Item> items {
        { "01-12edo-maior-natural.wav", 12, { 0, 4, 7, 12 }, 0.0 },
        { "02-19edo-maior-natural.wav", 19, { 0, 6, 11, 19 }, 0.0 },
        { "03-19edo-maior-alinhado.wav", 19, { 0, 6, 11, 19 }, 1.0 },
        { "04-10edo-natural.wav", 10, { 0, 3, 6, 10 }, 0.0 },
        { "05-10edo-alinhado.wav", 10, { 0, 3, 6, 10 }, 1.0 },
        { "06-7edo-natural.wav", 7, { 0, 2, 4, 7 }, 0.0 },
        { "07-7edo-alinhado.wav", 7, { 0, 2, 4, 7 }, 1.0 },
    };

    for (const auto& it : items)
    {
        const EdoScale scale { it.edo };
        std::vector<double> cents;
        for (int s : it.steps) cents.push_back (s * scale.step());
        writeWav (dir / it.file, renderChord (cents, scale, it.align));
    }
    std::printf ("\nAudios de comparacao em: %s\n", dir.string().c_str());
}

int main()
{
    cpuBenchmarks();
    dissonanceTables();
    renderAudio();
    return 0;
}
