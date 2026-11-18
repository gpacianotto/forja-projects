// Testes do botão No Fund (RF22): tirar a fundamental (1º harmônico) da onda do oscilador.
// Nomes de TEST_CASE sem acento: o ctest no Windows não os encontra se tiverem.
#include "AlignedSpectrum.h"
#include "Oscillator.h"
#include "PartialBank.h"
#include "Wavetables.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{
    // Janela de análise: 2048 amostras. Com o oscilador andando `cycles` ciclos nessa janela
    // (incremento = cycles / 2048, exato em double), o harmônico h cai certinho no bin
    // h × cycles da DFT, sem vazamento.
    constexpr int windowSize = Wavetables::tableSize;

    // Amplitude do harmônico h (pico da senoide) num trecho com `cycles` ciclos.
    double harmonic (const std::vector<float>& x, int cycles, int h)
    {
        const int bin = h * cycles;
        std::complex<double> sum;
        for (int n = 0; n < windowSize; ++n)
        {
            // (bin × n) mod N: ângulo exato, sem perder precisão com números grandes.
            const double angle = -2.0 * std::numbers::pi * static_cast<double> ((static_cast<long long> (bin) * n) % windowSize) / windowSize;
            sum += static_cast<double> (x[static_cast<size_t> (n)]) * std::polar (1.0, angle);
        }
        return 2.0 * std::abs (sum) / windowSize;
    }

    std::vector<float> render (Oscillator& osc, int cycles, float cut)
    {
        // Frequência = cycles Hz numa taxa de 2048 Hz: incremento cycles / 2048.
        osc.setFrequency (cycles, windowSize);
        osc.resetPhase();
        std::vector<float> out (windowSize);
        for (auto& sample : out)
            sample = osc.nextSample (cut);
        return out;
    }

    // Ciclo desenhado qualquer, sem simetria (fundamental com fase "torta"), para a forma Custom.
    std::vector<float> drawnCycle()
    {
        std::vector<float> cycle (windowSize);
        for (int i = 0; i < windowSize; ++i)
        {
            const double x = static_cast<double> (i) / windowSize;
            cycle[static_cast<size_t> (i)] = static_cast<float> (0.7 * std::cos (2.0 * std::numbers::pi * x + 0.4)
                                                               + 0.3 * std::sin (6.0 * std::numbers::pi * x)
                                                               + (x < 0.3 ? 0.5 : -0.2));
        }
        return cycle;
    }

    // Confere: sem a fundamental, o 1º harmônico some e do 2º ao 12º nada muda.
    void checkCut (Oscillator& osc, int cycles)
    {
        const auto full = render (osc, cycles, 0.0f);
        const auto cut  = render (osc, cycles, 1.0f);

        INFO ("ciclos na janela = " << cycles);
        REQUIRE (harmonic (full, cycles, 1) > 0.05);           // a forma tem fundamental
        CHECK (harmonic (cut, cycles, 1) < 1e-5);              // e ela sumiu

        for (int h = 2; h <= 12 && h * cycles < windowSize / 2; ++h)
            CHECK_THAT (harmonic (cut, cycles, h), WithinAbs (harmonic (full, cycles, h), 1e-5));
    }
}

TEST_CASE ("No Fund: formas prontas perdem so o 1o harmonico, em varios mipmaps")
{
    for (auto waveform : { Waveform::saw, Waveform::square, Waveform::triangle })
    {
        Oscillator osc;
        osc.setWaveform (waveform);
        INFO ("forma = " << static_cast<int> (waveform));

        // 1 ciclo: nível 0 (1023 harmônicos); 8 ciclos: nível 6; 64 ciclos: nível 12.
        for (int cycles : { 1, 8, 64 })
            checkCut (osc, cycles);
    }
}

TEST_CASE ("No Fund: forma desenhada (Custom) perde so o 1o harmonico, com qualquer fase")
{
    const auto cycle = drawnCycle();
    std::vector<float> mipmaps (Wavetables::mipmapSetSize);
    Wavetables::buildMipmaps (cycle.data(), mipmaps.data());

    Oscillator osc;
    osc.setCustomTables (mipmaps.data());
    osc.setWaveform (Waveform::custom);

    for (int cycles : { 1, 8, 64 })
        checkCut (osc, cycles);
}

TEST_CASE ("No Fund: desligado (0) soa identico ao oscilador de antes")
{
    Oscillator before, after;
    before.setWaveform (Waveform::square);
    after.setWaveform (Waveform::square);
    before.setFrequency (220.0, 48000.0);
    after.setFrequency (220.0, 48000.0);

    for (int i = 0; i < 4000; ++i)
        REQUIRE (before.nextSample() == after.nextSample (0.0f)); // igual bit a bit
}

TEST_CASE ("No Fund: no meio da rampa, a fundamental tem a fracao certa")
{
    Oscillator osc;
    osc.setWaveform (Waveform::saw);
    const double full = harmonic (render (osc, 8, 0.0f), 8, 1);

    CHECK_THAT (harmonic (render (osc, 8, 0.25f), 8, 1), WithinAbs (0.75 * full, 1e-5));
    CHECK_THAT (harmonic (render (osc, 8, 0.5f), 8, 1), WithinAbs (0.5 * full, 1e-5));
}

TEST_CASE ("No Fund: nota tao aguda que so cabe a fundamental fica muda")
{
    Oscillator osc;
    osc.setWaveform (Waveform::saw);
    osc.setFrequency (15000.0, 44100.0); // só o 1º harmônico abaixo de Nyquist (último nível)

    for (int i = 0; i < 500; ++i)
        CHECK_THAT (osc.nextSample (1.0f), WithinAbs (0.0, 1e-6));
}

TEST_CASE ("No Fund: lendo as tabelas do resto (Align, RF21) o oscilador ignora o corte")
{
    // No motor híbrido a fundamental está no banco de senoides, não na tabela.
    Oscillator a, b;
    for (auto* osc : { &a, &b })
    {
        osc->setWaveform (Waveform::saw);
        osc->setResidual (true);
        osc->setFrequency (110.0, 48000.0);
    }

    for (int i = 0; i < 2000; ++i)
        REQUIRE (a.nextSample (0.0f) == b.nextSample (1.0f));
}

TEST_CASE ("No Fund: banco de senoides (Align) baixa so a senoide 1")
{
    // Harmônicos naturais (1×, 2×, 3×...), 8 ciclos da fundamental por janela.
    AlignedSpectrum::Ratios natural;
    for (int h = 1; h <= AlignedSpectrum::numPartials; ++h)
        natural.ratio[static_cast<size_t> (h - 1)] = h;

    auto renderBank = [&] (float fundamentalGain)
    {
        PartialBank bank;
        bank.setRecipe (AlignedSpectrum::builtinRecipe (Waveform::saw));
        bank.setFrequency (8.0, natural, windowSize);
        bank.setFundamentalGain (fundamentalGain);
        bank.start (0.0);

        std::vector<float> out (windowSize);
        for (auto& sample : out)
            sample = bank.nextSample();
        return out;
    };

    const auto full = renderBank (1.0f);
    const auto half = renderBank (0.5f);
    const auto none = renderBank (0.0f);

    const double fundamental = harmonic (full, 8, 1);
    CHECK_THAT (fundamental, WithinAbs (2.0 / std::numbers::pi, 1e-3)); // amplitude da serra
    CHECK_THAT (harmonic (half, 8, 1), WithinAbs (0.5 * fundamental, 1e-4));
    CHECK (harmonic (none, 8, 1) < 1e-4);

    for (int h = 2; h <= 12; ++h)
        CHECK_THAT (harmonic (none, 8, h), WithinAbs (harmonic (full, 8, h), 1e-4));
}

TEST_CASE ("No Fund: trocar a receita mantem o volume da fundamental escolhido")
{
    // A receita muda quando o usuário redesenha a forma (RF15); o botão continua valendo.
    AlignedSpectrum::Ratios natural;
    for (int h = 1; h <= AlignedSpectrum::numPartials; ++h)
        natural.ratio[static_cast<size_t> (h - 1)] = h;

    PartialBank bank;
    bank.setRecipe (AlignedSpectrum::builtinRecipe (Waveform::saw));
    bank.setFrequency (8.0, natural, windowSize);
    bank.setFundamentalGain (0.0f);
    bank.setRecipe (AlignedSpectrum::builtinRecipe (Waveform::square));
    bank.start (0.0);

    std::vector<float> out (windowSize);
    for (auto& sample : out)
        sample = bank.nextSample();

    CHECK (harmonic (out, 8, 1) < 1e-4);
    CHECK_THAT (harmonic (out, 8, 3), WithinAbs (4.0 / (3.0 * std::numbers::pi), 1e-3)); // 3º da quadrada
}
