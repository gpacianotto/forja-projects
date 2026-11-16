// Testes do gráfico dos harmônicos (RF21, etapa 24): o que ele mostra tem de ser o que a voz toca.
// Nomes de TEST_CASE sem acento: o ctest no Windows não os encontra se tiverem.
#include "HarmonicsView.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>

using Catch::Matchers::WithinAbs;

namespace
{
    TuningEngine edoEngine (int edo)
    {
        TuningEngine engine;
        TuningEngine::Settings settings;
        settings.edo = edo;
        engine.setSettings (settings);
        return engine;
    }
}

TEST_CASE ("Grafico dos harmonicos: 12 EDO tem uma linha por semitom ate o 24o harmonico")
{
    HarmonicsView::Snapshot s;
    HarmonicsView::compute (s, edoEngine (12), 60, 0, 1.0);

    CHECK (s.lineCount == 55); // 100 ¢ ... 5500 ¢ (o eixo vai até ~5502 ¢)
    CHECK_THAT (s.lineCents[0], WithinAbs (100.0, 1e-9));
    CHECK_THAT (s.lineCents[54], WithinAbs (5500.0, 1e-9));
    CHECK (s.ratios.alignedCount == 22);
}

TEST_CASE ("Grafico dos harmonicos: mesmas razoes que a voz calcula")
{
    // A voz alinha pela altura do oscilador: nota + steps do oscilador.
    const auto engine = edoEngine (7);
    HarmonicsView::Snapshot s;
    HarmonicsView::compute (s, engine, 62, 3, 0.5);

    const double base = engine.centsFromBase (62.0, 0.0, 3);
    const auto expected = AlignedSpectrum::align ([&] (int k) { return engine.centsFromBase (62.0, 0.0, 3 + k) - base; }, 0.5);
    CHECK (s.ratios == expected);
    CHECK_THAT (s.align, WithinAbs (0.5, 1e-12));
}

TEST_CASE ("Grafico dos harmonicos: afinacao desenhada vista da nota tocada")
{
    // 7 EDO com o step 4 em 702 ¢: vista da nota do step 4, as alturas acima mudam.
    TuningEngine engine;
    TuningEngine::Settings settings;
    settings.edo = 7;
    settings.refNote = 60;
    settings.table = TuningTable::equal (7);
    settings.table.moveStep (4, 702.0, 7);
    engine.setSettings (settings);

    HarmonicsView::Snapshot fromRoot, fromFifth;
    HarmonicsView::compute (fromRoot, engine, 60, 0, 1.0);
    HarmonicsView::compute (fromFifth, engine, 64, 0, 1.0);

    CHECK_THAT (fromRoot.lineCents[3], WithinAbs (702.0, 1e-9));           // step 4 acima da raiz
    CHECK_THAT (fromFifth.lineCents[2], WithinAbs (1200.0 - 702.0, 1e-9)); // da quinta até a oitava
}

TEST_CASE ("Grafico dos harmonicos: volume junta seno e cosseno")
{
    AlignedSpectrum::Recipe recipe;
    recipe.sinAmp[2] = 0.3f;
    recipe.cosAmp[2] = 0.4f;
    CHECK_THAT (HarmonicsView::amplitude (recipe, 3), WithinAbs (0.5, 1e-6));

    const auto saw = AlignedSpectrum::builtinRecipe (Waveform::saw);
    CHECK_THAT (HarmonicsView::amplitude (saw, 2), WithinAbs (1.0 / std::numbers::pi, 1e-6));
}
