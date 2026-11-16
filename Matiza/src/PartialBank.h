#pragma once

#include <array>
#include <cmath>
#include <numbers>

#if defined(_M_X64) || defined(__x86_64__)
 #include <xmmintrin.h> // SSE: 4 floats por instrução (todo processador x64 tem)
 #define MATIZA_PARTIALS_SSE 1
#else
 #define MATIZA_PARTIALS_SSE 0 // outras arquiteturas (ex.: Mac ARM): versão escalar
#endif

#include "AlignedSpectrum.h"
#include "Wavetables.h"

// Banco de senoides de uma cópia de oscilador (RF21, harmônicos alinhados). C++ puro, sem
// alocação: seguro na thread de áudio.
//
// Cada harmônico é um vetor (cos, sen) que gira um ângulo fixo a cada amostra: girar custa 4
// multiplicações e 2 somas, sem chamar std::sin. Os dados ficam em arrays separados (structure
// of arrays), para a CPU girar 4 harmônicos de uma vez com SSE (como a Vector API do Java).
// Medido no estudo: 24 harmônicos × 192 cópias ≈ 6,5 % de um núcleo.
//
// Junto com o wavetable "do resto" (Oscillator::setResidual), soma a onda completa: os 24
// primeiros harmônicos aqui, nas posições alinhadas; os de cima no wavetable.
class PartialBank
{
public:
    static constexpr int size = AlignedSpectrum::numPartials;
    static_assert (size % 4 == 0, "o SSE processa 4 harmônicos por vez");
    static_assert (Wavetables::residualFirstHarmonic == size + 1, "o resto começa onde o banco acaba");

    // Começa na fase do oscilador (0..1 da fundamental): o harmônico h começa em h × fase, como
    // na tabela de onda. Assim o banco + o wavetable do resto continuam a onda sem salto.
    // Chama std::cos/sin 24 vezes: só no note-on ou ao ligar o alinhamento.
    void start (double phase) noexcept
    {
        constexpr double twoPi = 2.0 * std::numbers::pi;
        for (int k = 0; k < size; ++k)
        {
            const double angle = twoPi * (k + 1) * (phase - std::floor (phase));
            c[k] = static_cast<float> (std::cos (angle));
            s[k] = static_cast<float> (std::sin (angle));
        }
    }

    // Volume e fase de cada harmônico (receita da forma de onda). Barato se nada mudou.
    void setRecipe (const AlignedSpectrum::Recipe& newRecipe) noexcept
    {
        if (newRecipe == recipe)
            return;
        recipe = newRecipe;
        updateAmplitudes();
    }

    // Frequência da fundamental e posição de cada harmônico. Recalcula o giro de cada um
    // (std::cos e std::sin, a parte cara) só se algo mudou.
    void setFrequency (double hz, const AlignedSpectrum::Ratios& newRatios, double sampleRate) noexcept
    {
        if (hz == currentHz && sampleRate == currentSampleRate && newRatios.ratio == currentRatios)
            return;

        currentHz = hz;
        currentSampleRate = sampleRate;
        currentRatios = newRatios.ratio;

        constexpr double twoPi = 2.0 * std::numbers::pi;
        for (int k = 0; k < size; ++k)
        {
            const double cyclesPerSample = sampleRate > 0.0 ? hz * currentRatios[static_cast<size_t> (k)] / sampleRate : 0.0;

            // Harmônico acima de Nyquist (ou frequência inválida) fica mudo: viraria aliasing.
            audible[k] = cyclesPerSample > 0.0 && cyclesPerSample < maxCyclesPerSample;
            const double angle = audible[k] ? twoPi * cyclesPerSample : 0.0;
            rc[k] = static_cast<float> (std::cos (angle));
            rs[k] = static_cast<float> (std::sin (angle));
        }
        updateAmplitudes();
    }

    // Em float, o vetor de cada harmônico perde ou ganha tamanho aos poucos (arredondamento).
    // Uma correção a cada bloco de modulação (aproximação de 1/sqrt(c² + s²) perto de 1) mantém
    // o volume estável: no estudo, erro de −79 dB depois de 10 s.
    void renormalise() noexcept
    {
        for (int k = 0; k < size; ++k)
        {
            const float g = 1.5f - 0.5f * (c[k] * c[k] + s[k] * s[k]);
            c[k] *= g;
            s[k] *= g;
        }
    }

    // Amostra atual (soma dos harmônicos) e avança um passo.
    float nextSample() noexcept
    {
       #if MATIZA_PARTIALS_SSE
        __m128 acc = _mm_setzero_ps();
        for (int k = 0; k < size; k += 4)
        {
            const __m128 C = _mm_load_ps (c + k), S = _mm_load_ps (s + k);
            acc = _mm_add_ps (acc, _mm_add_ps (_mm_mul_ps (S, _mm_load_ps (ampSin + k)),
                                               _mm_mul_ps (C, _mm_load_ps (ampCos + k))));

            // Gira: (c, s) × (rc, rs), multiplicação de números complexos.
            const __m128 RC = _mm_load_ps (rc + k), RS = _mm_load_ps (rs + k);
            _mm_store_ps (c + k, _mm_sub_ps (_mm_mul_ps (C, RC), _mm_mul_ps (S, RS)));
            _mm_store_ps (s + k, _mm_add_ps (_mm_mul_ps (C, RS), _mm_mul_ps (S, RC)));
        }

        // Soma os 4 floats do acumulador.
        __m128 shuffled = _mm_shuffle_ps (acc, acc, _MM_SHUFFLE (2, 3, 0, 1));
        __m128 sums = _mm_add_ps (acc, shuffled);
        shuffled = _mm_movehl_ps (shuffled, sums);
        return _mm_cvtss_f32 (_mm_add_ss (sums, shuffled));
       #else
        float out = 0.0f;
        for (int k = 0; k < size; ++k)
        {
            out += s[k] * ampSin[k] + c[k] * ampCos[k];
            const float nc = c[k] * rc[k] - s[k] * rs[k];
            s[k] = c[k] * rs[k] + s[k] * rc[k];
            c[k] = nc;
        }
        return out;
       #endif
    }

    // Os mipmaps guardam harmônicos até exatamente Nyquist (0,5 ciclo por amostra); aqui uma
    // pequena folga, porque o alinhamento pode subir um harmônico alguns cents.
    static constexpr double maxCyclesPerSample = 0.49;

private:
    void updateAmplitudes() noexcept
    {
        for (int k = 0; k < size; ++k)
        {
            ampSin[k] = audible[k] ? recipe.sinAmp[static_cast<size_t> (k)] : 0.0f;
            ampCos[k] = audible[k] ? recipe.cosAmp[static_cast<size_t> (k)] : 0.0f;
        }
    }

    // alignas(16): o SSE lê 16 bytes alinhados de uma vez.
    alignas (16) float c[size] {};      // cosseno da fase de cada harmônico
    alignas (16) float s[size] {};      // seno da fase
    alignas (16) float rc[size] {};     // cosseno do giro por amostra
    alignas (16) float rs[size] {};     // seno do giro por amostra
    alignas (16) float ampSin[size] {}; // volume (receita × audível)
    alignas (16) float ampCos[size] {};
    bool audible[size] {};

    AlignedSpectrum::Recipe recipe;
    std::array<double, size> currentRatios {};
    double currentHz = -1.0, currentSampleRate = 0.0;
};
