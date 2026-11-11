// Benchmark do estudo de wavetable: oscilador do Matiza x protótipo de wavetable com mipmaps.
// (No estudo, o oscilador do Matiza ainda era PolyBLEP; desde o RF13 ele também é wavetable.)
// Mede CPU (192 osciladores = 8 vozes x 3 osc x 8 cópias) e aliasing (energia fora dos harmônicos).
#include "Oscillator.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <numbers>
#include <vector>

constexpr double sampleRate = 48000.0;
constexpr double pi = std::numbers::pi;

// ---------------------------------------------------------------- wavetable com mipmaps
struct Wavetable
{
    static constexpr int size = 2048;
    static constexpr int numLevels = 11; // uma tabela por oitava
    std::vector<std::array<float, size + 1>> levels; // +1: cópia da 1ª amostra para interpolar

    // Tabela k serve até a frequência baseHz * 2^k; tem harmônicos até Nyquist dessa frequência.
    static constexpr double baseHz = 20.0;

    template <typename HarmonicAmp>
    explicit Wavetable (HarmonicAmp amp)
    {
        levels.resize (numLevels);
        for (int k = 0; k < numLevels; ++k)
        {
            const double topHz = baseHz * std::pow (2.0, k + 1);
            int maxHarm = static_cast<int> ((sampleRate * 0.5) / topHz);
            maxHarm = std::max (1, std::min (maxHarm, size / 2 - 1));
            for (int i = 0; i <= size; ++i)
            {
                double t = static_cast<double> (i % size) / size, v = 0.0;
                for (int h = 1; h <= maxHarm; ++h)
                    v += amp (h) * std::sin (2.0 * pi * h * t);
                levels[k][i] = static_cast<float> (v);
            }
        }
    }
};

struct WavetableOsc
{
    const Wavetable* table = nullptr;
    double phase = 0.0, increment = 0.0;
    const float* current = nullptr;

    void setFrequency (double hz)
    {
        increment = hz / sampleRate;
        int level = static_cast<int> (std::ceil (std::log2 (std::max (hz, Wavetable::baseHz) / Wavetable::baseHz))) - 1;
        level = std::clamp (level, 0, Wavetable::numLevels - 1);
        current = table->levels[level].data();
    }

    float next()
    {
        const double pos = phase * Wavetable::size;
        const int i = static_cast<int> (pos);
        const float frac = static_cast<float> (pos - i);
        const float out = current[i] + frac * (current[i + 1] - current[i]);
        phase += increment;
        if (phase >= 1.0) phase -= 1.0;
        return out;
    }
};

// ---------------------------------------------------------------- FFT para medir aliasing
void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * pi / static_cast<double> (len);
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0);
            for (size_t j = 0; j < len / 2; ++j)
            {
                auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

// Razão (em dB) entre a energia fora dos harmônicos (aliasing) e a energia total.
template <typename Gen>
double aliasingDb (Gen gen, double hz)
{
    constexpr size_t n = 1 << 16;
    std::vector<std::complex<double>> buf (n);
    for (size_t i = 0; i < n; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * pi * i / (n - 1)); // janela de Hann
        buf[i] = gen() * w;
    }
    fft (buf);
    double total = 0.0, alias = 0.0;
    const double binHz = sampleRate / n;
    for (size_t b = 1; b < n / 2; ++b)
    {
        const double p = std::norm (buf[b]);
        total += p;
        const double f = b * binHz;
        const double nearestHarm = std::round (f / hz) * hz;
        if (std::abs (f - nearestHarm) > 4 * binHz) // longe de qualquer harmônico = aliasing
            alias += p;
    }
    return 10.0 * std::log10 (alias / total + 1e-30);
}

template <typename Fn>
double timeIt (Fn fn)
{
    const auto t0 = std::chrono::high_resolution_clock::now();
    fn();
    const auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double> (t1 - t0).count();
}

int main()
{
    const Wavetable sawTable ([] (int h) { return (h % 2 ? 1.0 : -1.0) * 2.0 / (pi * h); });
    const Wavetable sineTable ([] (int h) { return h == 1 ? 1.0 : 0.0; });

    constexpr int numOsc = 192;
    constexpr int seconds = 1;
    const int numSamples = static_cast<int> (sampleRate) * seconds;
    volatile float sink = 0.0f;

    auto benchPolyBlep = [&] (Waveform wf)
    {
        std::vector<Oscillator> oscs (numOsc);
        for (int o = 0; o < numOsc; ++o) { oscs[o].setWaveform (wf); oscs[o].setFrequency (110.0 + o, sampleRate); }
        return timeIt ([&] {
            float acc = 0.0f;
            for (int s = 0; s < numSamples; ++s)
                for (auto& osc : oscs) acc += osc.nextSample();
            sink = acc;
        });
    };

    auto benchTable = [&] (const Wavetable& table)
    {
        std::vector<WavetableOsc> oscs (numOsc);
        for (int o = 0; o < numOsc; ++o) { oscs[o].table = &table; oscs[o].setFrequency (110.0 + o); }
        return timeIt ([&] {
            float acc = 0.0f;
            for (int s = 0; s < numSamples; ++s)
                for (auto& osc : oscs) acc += osc.next();
            sink = acc;
        });
    };

    std::printf ("CPU: 192 osciladores, %d s de audio a 48 kHz (%% de um nucleo)\n", seconds);
    std::printf ("  Matiza sine       : %6.1f %%\n", 100.0 * benchPolyBlep (Waveform::sine) / seconds);
    std::printf ("  Matiza saw        : %6.1f %%\n", 100.0 * benchPolyBlep (Waveform::saw) / seconds);
    std::printf ("  Matiza square     : %6.1f %%\n", 100.0 * benchPolyBlep (Waveform::square) / seconds);
    std::printf ("  Prototipo (saw)   : %6.1f %%\n", 100.0 * benchTable (sawTable) / seconds);
    std::printf ("  Prototipo (sine)  : %6.1f %%\n", 100.0 * benchTable (sineTable) / seconds);

    const std::size_t tableBytes = Wavetable::numLevels * (Wavetable::size + 1) * sizeof (float);
    std::printf ("\nMemoria por forma de onda com mipmaps: %.1f KB\n", tableBytes / 1024.0);
    std::printf ("Tabela de 256 frames (estilo Serum): %.1f MB\n", 256.0 * tableBytes / (1024.0 * 1024.0));

    std::printf ("\nAliasing da serra (energia fora dos harmonicos, quanto menor melhor)\n");
    for (double hz : { 440.0, 1760.0, 3520.0, 7040.0 })
    {
        Oscillator pb; pb.setWaveform (Waveform::saw); pb.setFrequency (hz, sampleRate);
        WavetableOsc wt; wt.table = &sawTable; wt.setFrequency (hz);
        double ph = 0.0;
        const double a = aliasingDb ([&] { return pb.nextSample(); }, hz);
        const double b = aliasingDb ([&] { return wt.next(); }, hz);
        const double c = aliasingDb ([&] { const double v = 2.0 * ph - 1.0; ph += hz / sampleRate; ph -= std::floor (ph); return v; }, hz);
        std::printf ("  %6.0f Hz: ingenua %6.1f dB | Matiza %6.1f dB | prototipo %6.1f dB\n", hz, c, a, b);
    }
    return 0;
}
