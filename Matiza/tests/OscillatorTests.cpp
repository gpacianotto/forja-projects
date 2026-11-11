// Nomes de TEST_CASE sem acento: o ctest no Windows não os encontra se tiverem.
#include "Oscillator.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double sampleRate = 48000.0;

    std::vector<float> render (Waveform wave, double hz, int numSamples)
    {
        Oscillator osc;
        osc.setWaveform (wave);
        osc.setFrequency (hz, sampleRate);

        std::vector<float> out (static_cast<size_t> (numSamples));
        for (auto& s : out)
            s = osc.nextSample();
        return out;
    }

    float maxStep (const std::vector<float>& signal)
    {
        float result = 0.0f;
        for (size_t i = 1; i < signal.size(); ++i)
            result = std::max (result, std::abs (signal[i] - signal[i - 1]));
        return result;
    }

    double mean (const std::vector<float>& signal)
    {
        double sum = 0.0;
        for (float s : signal)
            sum += s;
        return sum / static_cast<double> (signal.size());
    }
}

TEST_CASE ("Senoide em sampleRate/4 passa por 0, 1, 0, -1")
{
    const auto s = render (Waveform::sine, sampleRate / 4.0, 4);

    CHECK_THAT (s[0], WithinAbs (0.0, 1e-6));
    CHECK_THAT (s[1], WithinAbs (1.0, 1e-6));
    CHECK_THAT (s[2], WithinAbs (0.0, 1e-6));
    CHECK_THAT (s[3], WithinAbs (-1.0, 1e-6));
}

// Limite de 1,3 (e não 1,0): serra e quadrada limitadas em banda passam de 1 perto do salto
// (fenômeno de Gibbs, até ~18 %). Numa nota bem aguda sobra só o 1º harmônico, e a quadrada vira
// uma senoide de amplitude 4/π ≈ 1,27. É o preço de não ter aliasing.
TEST_CASE ("Todas as formas de onda ficam dentro de [-1.3, 1.3] e com media perto de 0")
{
    for (auto wave : { Waveform::sine, Waveform::saw, Waveform::square, Waveform::triangle })
    {
        for (double hz : { 30.0, 440.0, 5000.0, 15000.0 })
        {
            INFO ("onda = " << static_cast<int> (wave) << ", hz = " << hz);
            // 1 segundo inteiro = número inteiro de ciclos, então a média deve ser ~0.
            const auto s = render (wave, hz, static_cast<int> (sampleRate));

            CHECK (*std::max_element (s.begin(), s.end()) <= 1.3f);
            CHECK (*std::min_element (s.begin(), s.end()) >= -1.3f);
            CHECK_THAT (mean (s), WithinAbs (0.0, 0.01));
        }
    }
}

TEST_CASE ("Serra e quadrada limitadas em banda nao saltam de uma amostra para a outra")
{
    // Uma serra ou quadrada "ingênua" salta 2,0 de uma amostra para a outra.
    // Lida da tabela certa (mipmap), o salto vira uma subida rápida, mas contínua.
    CHECK (maxStep (render (Waveform::saw, 1000.0, 4800)) < 1.5f);
    CHECK (maxStep (render (Waveform::square, 1000.0, 4800)) < 1.5f);
}

TEST_CASE ("Fase continua valida mesmo com frequencia acima da taxa de amostragem")
{
    // Não importa o som (a guarda de Nyquist, RF4, impede essas notas); só não pode
    // gerar NaN/infinito nem números enormes.
    for (auto wave : { Waveform::sine, Waveform::saw, Waveform::square, Waveform::triangle })
    {
        const auto s = render (wave, sampleRate * 1.7, 1000);

        for (float x : s)
        {
            REQUIRE (std::isfinite (x));
            REQUIRE (std::abs (x) <= 3.0f);
        }
    }
}

TEST_CASE ("Formas de onda iguais as do oscilador antigo (mesma fase e sentido)")
{
    // Nota grave (55 Hz): tabela com muitos harmônicos, perto da forma ideal.
    // 48000 / 55 não é inteiro, então comparamos pela fase: amostra n = fase n × 55 / 48000.
    const double hz = 55.0;
    const auto at = [hz] (Waveform wave, double phase)
    {
        Oscillator osc;
        osc.setWaveform (wave);
        osc.setFrequency (hz, sampleRate);
        osc.setPhase (phase);
        return osc.nextSample();
    };

    CHECK_THAT (at (Waveform::sine,     0.25), WithinAbs (1.0, 1e-3));
    CHECK_THAT (at (Waveform::saw,      0.25), WithinAbs (-0.5, 0.01)); // rampa 2t - 1
    CHECK_THAT (at (Waveform::saw,      0.75), WithinAbs (0.5, 0.01));
    CHECK_THAT (at (Waveform::square,   0.25), WithinAbs (1.0, 0.01));
    CHECK_THAT (at (Waveform::square,   0.75), WithinAbs (-1.0, 0.01));
    CHECK_THAT (at (Waveform::triangle, 0.0),  WithinAbs (0.0, 0.01));
    CHECK_THAT (at (Waveform::triangle, 0.25), WithinAbs (-1.0, 0.01));
    CHECK_THAT (at (Waveform::triangle, 0.75), WithinAbs (1.0, 0.01));
}
