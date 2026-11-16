// Testes dos harmônicos alinhados à escala (RF21, etapa 22 do plano).
// Nomes de TEST_CASE sem acento: o ctest no Windows não os encontra se tiverem.
#include "AlignedSpectrum.h"
#include "TuningTable.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace
{
    AlignedSpectrum::Ratios alignEdo (int edo, double amount)
    {
        const TuningTable none; // afinação igual
        return AlignedSpectrum::align ([&] (int k) { return none.stepsToCents (k, edo); }, amount);
    }

    double ratioOf (const AlignedSpectrum::Ratios& r, int h) { return r.ratio[static_cast<size_t> (h - 1)]; }
}

TEST_CASE ("Alinhamento: Align 0 deixa os harmonicos naturais")
{
    const auto r = alignEdo (7, 0.0);
    for (int h = 1; h <= AlignedSpectrum::numPartials; ++h)
        CHECK_THAT (ratioOf (r, h), WithinAbs (h, 1e-9));
    CHECK (r.alignedCount == 14); // o limite de colisão não depende do Align
}

TEST_CASE ("Alinhamento: 12 EDO puxa a terca para o semitom")
{
    const auto r = alignEdo (12, 1.0);
    CHECK_THAT (ratioOf (r, 1), WithinAbs (1.0, 1e-9));
    CHECK_THAT (ratioOf (r, 3), WithinAbs (std::exp2 (19.0 / 12.0), 1e-9)); // 1902 ¢ -> 1900 ¢
    CHECK_THAT (ratioOf (r, 5), WithinAbs (std::exp2 (28.0 / 12.0), 1e-9)); // 2786 ¢ -> 2800 ¢
}

TEST_CASE ("Alinhamento: oitavas nao mudam")
{
    for (int edo : { 5, 7, 12, 19, 31 })
    {
        const auto r = alignEdo (edo, 1.0);
        CHECK_THAT (ratioOf (r, 2), WithinAbs (2.0, 1e-9));
        CHECK_THAT (ratioOf (r, 4), WithinAbs (4.0, 1e-9));
        CHECK_THAT (ratioOf (r, 8), WithinAbs (8.0, 1e-9));
    }
}

TEST_CASE ("Alinhamento: 19 EDO bate com o estudo")
{
    const auto r = alignEdo (19, 1.0);
    CHECK_THAT (ratioOf (r, 3), WithinAbs (std::exp2 (30.0 / 19.0), 1e-9)); // 2,988
    CHECK_THAT (ratioOf (r, 7), WithinAbs (std::exp2 (53.0 / 19.0), 1e-9)); // 6,914
    CHECK_THAT (ratioOf (r, 3), WithinAbs (2.988, 1e-3));
    CHECK (r.alignedCount == 24); // limite de colisão da 19 EDO (34) passa dos 24 harmônicos
}

TEST_CASE ("Alinhamento: limite de colisao por EDO")
{
    CHECK (alignEdo (5, 1.0).alignedCount == 10);
    CHECK (alignEdo (7, 1.0).alignedCount == 14);
    CHECK (alignEdo (12, 1.0).alignedCount == 22);
    CHECK (alignEdo (72, 1.0).alignedCount == 24);

    // Acima do limite, os harmônicos ficam naturais.
    const auto r = alignEdo (5, 1.0);
    CHECK_THAT (ratioOf (r, 11), WithinAbs (11.0, 1e-9));
    CHECK_THAT (ratioOf (r, 24), WithinAbs (24.0, 1e-9));
}

TEST_CASE ("Alinhamento: Align 50 por cento fica no meio em cents")
{
    const auto r = alignEdo (12, 0.5);
    const double natural = 1200.0 * std::log2 (3.0);
    CHECK_THAT (1200.0 * std::log2 (ratioOf (r, 3)), WithinAbs (natural + 0.5 * (1900.0 - natural), 1e-9));

    // Fora de 0..1 vale a ponta.
    CHECK (alignEdo (12, 2.0) == alignEdo (12, 1.0));
    CHECK (alignEdo (12, -1.0) == alignEdo (12, 0.0));
}

TEST_CASE ("Alinhamento: harmonicos alinhados continuam em ordem")
{
    for (int edo : { 5, 7, 10, 12, 13, 19, 22, 31, 53, 72 })
    {
        const auto r = alignEdo (edo, 1.0);
        for (int h = 2; h <= AlignedSpectrum::numPartials; ++h)
            CHECK (ratioOf (r, h) > ratioOf (r, h - 1));
    }
}

TEST_CASE ("Alinhamento: afinacao desenhada (RF17)")
{
    // 7 EDO com o step 4 puxado para a quinta justa (702 ¢): o 3º harmônico (1902 ¢) cai
    // exatamente nela, uma oitava acima (1200 + 702).
    TuningTable table = TuningTable::equal (7);
    table.moveStep (4, 702.0, 7);

    const auto r = AlignedSpectrum::align ([&] (int k) { return table.stepsToCents (k, 7); }, 1.0);
    CHECK_THAT (ratioOf (r, 3), WithinAbs (std::exp2 (1902.0 / 1200.0), 1e-9));
}

TEST_CASE ("Alinhamento: fim da afinacao (NaN) para o alinhamento")
{
    // Tabela de master com 12-TET até o step 20 e nada depois.
    const auto r = AlignedSpectrum::align ([] (int k)
    {
        return k <= 20 ? 100.0 * k : std::numeric_limits<double>::quiet_NaN();
    }, 1.0);

    CHECK (r.alignedCount == 3); // o 4º (2400 ¢) precisaria do step 21
    CHECK_THAT (ratioOf (r, 3), WithinAbs (std::exp2 (19.0 / 12.0), 1e-9));
    CHECK_THAT (ratioOf (r, 4), WithinAbs (4.0, 1e-9));
}

TEST_CASE ("Receitas: formas prontas usam os coeficientes do wavetable")
{
    const auto square = AlignedSpectrum::builtinRecipe (Waveform::square);
    CHECK_THAT (square.sinAmp[0], WithinAbs (4.0 / std::numbers::pi, 1e-6));
    CHECK_THAT (square.sinAmp[1], WithinAbs (0.0, 1e-9));
    CHECK_THAT (square.sinAmp[2], WithinAbs (4.0 / (3.0 * std::numbers::pi), 1e-6));

    const auto sine = AlignedSpectrum::builtinRecipe (Waveform::sine);
    CHECK_THAT (sine.sinAmp[0], WithinAbs (1.0, 1e-9));
    CHECK_THAT (sine.sinAmp[1], WithinAbs (0.0, 1e-9));
}

TEST_CASE ("Receitas: a serra do banco de wavetables volta como a formula")
{
    // Prova que a DFT da forma desenhada usa a mesma convenção de sinal e fase do banco.
    const auto fromTable = AlignedSpectrum::recipeFromCycle (Wavetables::table (Waveform::saw, 0));
    const auto formula = AlignedSpectrum::builtinRecipe (Waveform::saw);
    for (size_t i = 0; i < AlignedSpectrum::numPartials; ++i)
    {
        CHECK_THAT (fromTable.sinAmp[i], WithinAbs (formula.sinAmp[i], 1e-4));
        CHECK_THAT (fromTable.cosAmp[i], WithinAbs (0.0, 1e-4));
    }
}

TEST_CASE ("Receitas: onda desenhada com fase de cosseno")
{
    std::vector<float> cycle (Wavetables::tableSize);
    for (int i = 0; i < Wavetables::tableSize; ++i)
        cycle[static_cast<size_t> (i)] = static_cast<float> (0.5 * std::cos (2.0 * std::numbers::pi * 3 * i / Wavetables::tableSize));

    const auto r = AlignedSpectrum::recipeFromCycle (cycle.data());
    CHECK_THAT (r.cosAmp[2], WithinAbs (0.5, 1e-5));
    CHECK_THAT (r.sinAmp[2], WithinAbs (0.0, 1e-5));
    CHECK_THAT (r.cosAmp[0], WithinAbs (0.0, 1e-5));
}
