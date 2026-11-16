// Testes do motor híbrido dos harmônicos alinhados (RF21, etapa 23 do plano): banco de
// senoides + wavetable "do resto", e o contador de revisão da afinação.
// Nomes de TEST_CASE sem acento: o ctest no Windows não os encontra se tiverem.
#include "AlignedSpectrum.h"
#include "Oscillator.h"
#include "PartialBank.h"
#include "TuningEngine.h"
#include "TuningTable.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int modInterval = 32; // a voz renormaliza o banco a cada 32 amostras

    AlignedSpectrum::Ratios ratiosFor (int edo, double amount)
    {
        const TuningTable none;
        return AlignedSpectrum::align ([&] (int k) { return none.stepsToCents (k, edo); }, amount);
    }

    // Maior diferença entre a onda completa e (wavetable do resto + banco), começando na fase dada.
    double hybridError (Waveform waveform, double hz, double startPhase,
                        const float* customFull = nullptr, const float* customResidual = nullptr,
                        const AlignedSpectrum::Recipe* customRecipe = nullptr)
    {
        Oscillator full, rest;
        full.setWaveform (waveform);
        rest.setWaveform (waveform);
        full.setCustomTables (customFull, customResidual);
        rest.setCustomTables (customFull, customResidual);
        rest.setResidual (true);
        full.setPhase (startPhase);
        rest.setPhase (startPhase);
        full.setFrequency (hz, sampleRate);
        rest.setFrequency (hz, sampleRate);

        PartialBank bank;
        bank.setRecipe (customRecipe != nullptr ? *customRecipe : AlignedSpectrum::builtinRecipe (waveform));
        bank.start (startPhase);
        bank.setFrequency (hz, ratiosFor (12, 0.0), sampleRate); // Align 0: harmônicos naturais

        double maxError = 0.0;
        for (int i = 0; i < 4800; ++i)
        {
            if (i % modInterval == 0)
                bank.renormalise();
            const double expected = full.nextSample();
            const double hybrid = rest.nextSample() + bank.nextSample();
            maxError = std::max (maxError, std::abs (expected - hybrid));
        }
        return maxError;
    }
}

TEST_CASE ("Motor hibrido: resto + senoides = onda completa (formas prontas)")
{
    for (auto waveform : { Waveform::sine, Waveform::saw, Waveform::square, Waveform::triangle })
    {
        CHECK (hybridError (waveform, 110.0, 0.0) < 2e-3);
        CHECK (hybridError (waveform, 220.0, 0.37) < 2e-3); // começando no meio do ciclo
    }
}

TEST_CASE ("Motor hibrido: resto + senoides = onda completa (forma desenhada)")
{
    // Uma onda qualquer, com fase de cosseno e harmônicos pares e ímpares.
    std::vector<float> cycle (Wavetables::tableSize);
    for (int i = 0; i < Wavetables::tableSize; ++i)
    {
        const double t = static_cast<double> (i) / Wavetables::tableSize;
        cycle[static_cast<size_t> (i)] = static_cast<float> (t < 0.3 ? t / 0.3 : 0.4 * std::cos (6.0 * std::numbers::pi * t));
    }

    std::vector<float> full (Wavetables::mipmapSetSize), residual (Wavetables::mipmapSetSize);
    Wavetables::buildMipmaps (cycle.data(), full.data());
    Wavetables::buildMipmaps (cycle.data(), residual.data(), Wavetables::residualFirstHarmonic);
    const auto recipe = AlignedSpectrum::recipeFromCycle (cycle.data());

    CHECK (hybridError (Waveform::custom, 110.0, 0.0, full.data(), residual.data(), &recipe) < 2e-3);
    CHECK (hybridError (Waveform::custom, 330.0, 0.61, full.data(), residual.data(), &recipe) < 2e-3);
}

TEST_CASE ("Motor hibrido: notas agudas so tem o banco (resto vazio)")
{
    // Acima de ~1 kHz o mipmap não guarda o 25º harmônico: o resto fica em silêncio.
    Oscillator rest;
    rest.setWaveform (Waveform::saw);
    rest.setResidual (true);
    rest.setFrequency (2000.0, sampleRate);
    double peak = 0.0;
    for (int i = 0; i < 1000; ++i)
        peak = std::max (peak, static_cast<double> (std::abs (rest.nextSample())));
    CHECK (peak < 1e-6);
}

TEST_CASE ("Banco de senoides: harmonico acima de Nyquist fica mudo")
{
    AlignedSpectrum::Recipe onlyTop;
    onlyTop.sinAmp[23] = 1.0f; // 24º harmônico

    PartialBank bank;
    bank.setRecipe (onlyTop);
    bank.start (0.25);
    bank.setFrequency (1500.0, ratiosFor (12, 0.0), sampleRate); // 24 × 1500 = 36 kHz > Nyquist

    double peak = 0.0;
    for (int i = 0; i < 1000; ++i)
        peak = std::max (peak, static_cast<double> (std::abs (bank.nextSample())));
    CHECK (peak == 0.0);
}

TEST_CASE ("Banco de senoides: volume estavel depois de 10 s")
{
    PartialBank bank;
    bank.setRecipe (AlignedSpectrum::builtinRecipe (Waveform::sine));
    bank.start (0.0);
    bank.setFrequency (440.0, ratiosFor (12, 0.0), sampleRate);

    double peakLastSecond = 0.0;
    const int total = 10 * static_cast<int> (sampleRate);
    for (int i = 0; i < total; ++i)
    {
        if (i % modInterval == 0)
            bank.renormalise();
        const double v = bank.nextSample();
        if (i >= total - static_cast<int> (sampleRate))
            peakLastSecond = std::max (peakLastSecond, std::abs (v));
    }
    CHECK_THAT (peakLastSecond, WithinAbs (1.0, 1e-3));
}

TEST_CASE ("Banco de senoides: harmonico alinhado toca na frequencia alinhada")
{
    // 12 EDO, Align 100 %: o 3º harmônico de 100 Hz vai de 300 Hz para 100 × 2^(19/12) = 299,66 Hz.
    AlignedSpectrum::Recipe onlyThird;
    onlyThird.sinAmp[2] = 1.0f;

    PartialBank bank;
    bank.setRecipe (onlyThird);
    bank.start (0.0);
    bank.setFrequency (100.0, ratiosFor (12, 1.0), sampleRate);

    // Conta os ciclos em 10 s pelas passagens por zero subindo.
    int cycles = 0;
    float previous = 0.0f;
    for (int i = 0; i < 10 * static_cast<int> (sampleRate); ++i)
    {
        if (i % modInterval == 0)
            bank.renormalise();
        const float v = bank.nextSample();
        if (previous < 0.0f && v >= 0.0f)
            ++cycles;
        previous = v;
    }
    CHECK (std::abs (cycles / 10.0 - 100.0 * std::exp2 (19.0 / 12.0)) < 0.2);
}

TEST_CASE ("Afinacao: revisao muda so quando a afinacao muda")
{
    TuningEngine engine;
    const auto start = engine.getRevision();

    TuningEngine::Settings settings;
    engine.setSettings (settings); // igual ao padrão
    CHECK (engine.getRevision() == start);

    settings.edo = 19;
    engine.setSettings (settings);
    const auto afterEdo = engine.getRevision();
    CHECK (afterEdo != start);

    engine.setSettings (settings);
    CHECK (engine.getRevision() == afterEdo);

    TuningEngine::MasterTuning master;
    master.active = true;
    for (int n = 0; n < TuningEngine::numMidiNotes; ++n)
        master.hz[static_cast<size_t> (n)] = 440.0 * std::exp2 ((n - 69) / 12.0);
    engine.setMasterTuning (master);
    const auto afterMaster = engine.getRevision();
    CHECK (afterMaster != afterEdo);

    engine.setMasterTuning (master); // mesma tabela, outro bloco
    CHECK (engine.getRevision() == afterMaster);

    master.hz[60] *= 1.01;
    engine.setMasterTuning (master);
    CHECK (engine.getRevision() != afterMaster);
}
