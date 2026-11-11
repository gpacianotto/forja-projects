// Nomes de TEST_CASE sem acento: o ctest no Windows não os encontra se tiverem.
#include "StepMath.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE ("Steps: nota na Ref Note e step 0 da oitava 0 (RF16)")
{
    const auto pos = StepMath::locate (69, 69, 19);
    CHECK (pos.step == 0);
    CHECK (pos.octave == 0);
}

TEST_CASE ("Steps: notas acima da Ref Note")
{
    CHECK (StepMath::locate (75, 69, 19).step == 6);
    CHECK (StepMath::locate (75, 69, 19).octave == 0);

    // Uma oitava inteira acima volta ao step 0, oitava +1.
    CHECK (StepMath::locate (88, 69, 19).step == 0);
    CHECK (StepMath::locate (88, 69, 19).octave == 1);

    CHECK (StepMath::locate (90, 69, 19).step == 2);
    CHECK (StepMath::locate (90, 69, 19).octave == 1);
}

TEST_CASE ("Steps: notas abaixo da Ref Note tem step positivo e oitava negativa")
{
    // 68 = um step abaixo de 69: ultimo step da oitava de baixo.
    CHECK (StepMath::locate (68, 69, 12).step == 11);
    CHECK (StepMath::locate (68, 69, 12).octave == -1);

    // 57 = uma oitava abaixo em 12 EDO: step 0, oitava -1.
    CHECK (StepMath::locate (57, 69, 12).step == 0);
    CHECK (StepMath::locate (57, 69, 12).octave == -1);

    // 56 = 13 steps abaixo: step 11, oitava -2.
    CHECK (StepMath::locate (56, 69, 12).step == 11);
    CHECK (StepMath::locate (56, 69, 12).octave == -2);

    CHECK (StepMath::locate (0, 69, 72).step == 3);    // -69 = -72 + 3
    CHECK (StepMath::locate (0, 69, 72).octave == -1);
}

TEST_CASE ("Steps: 1 EDO tem so o step 0")
{
    for (int note = 0; note <= 127; ++note)
    {
        const auto pos = StepMath::locate (note, 69, 1);
        CHECK (pos.step == 0);
        CHECK (pos.octave == note - 69);
    }
}

TEST_CASE ("Steps: para toda nota, oitava*edo + step reconstroi a distancia ate a Ref Note")
{
    for (int edo = 1; edo <= 72; ++edo)
        for (int ref = 0; ref <= 127; ref += 9)
            for (int note = 0; note <= 127; ++note)
            {
                const auto pos = StepMath::locate (note, ref, edo);
                REQUIRE (pos.step >= 0);
                REQUIRE (pos.step < edo);
                REQUIRE (pos.octave * edo + pos.step == note - ref);
            }
}

TEST_CASE ("Steps: posicao em cents e equidistante")
{
    CHECK_THAT (StepMath::stepCents (0, 19), WithinAbs (0.0, 1e-9));
    CHECK_THAT (StepMath::stepCents (1, 19), WithinAbs (63.157894736, 1e-6));
    CHECK_THAT (StepMath::stepCents (6, 19), WithinAbs (378.947368421, 1e-6));
    CHECK_THAT (StepMath::stepCents (3, 12), WithinAbs (300.0, 1e-9));
    CHECK_THAT (StepMath::stepCents (36, 72), WithinAbs (600.0, 1e-9));
}

TEST_CASE ("Steps: wrapCents leva qualquer altura para 0..1200")
{
    CHECK_THAT (StepMath::wrapCents (0.0), WithinAbs (0.0, 1e-9));
    CHECK_THAT (StepMath::wrapCents (379.0), WithinAbs (379.0, 1e-9));
    CHECK_THAT (StepMath::wrapCents (1200.0), WithinAbs (0.0, 1e-9));
    CHECK_THAT (StepMath::wrapCents (1500.0), WithinAbs (300.0, 1e-9));
    CHECK_THAT (StepMath::wrapCents (-100.0), WithinAbs (1100.0, 1e-9));
    CHECK_THAT (StepMath::wrapCents (-2400.0), WithinAbs (0.0, 1e-9));

    const double tiny = StepMath::wrapCents (-1e-14);
    CHECK (tiny >= 0.0);
    CHECK (tiny < 1200.0);
}

TEST_CASE ("Steps: frequencia de uma posicao")
{
    CHECK_THAT (StepMath::toHz (440.0, 0, 0.0), WithinAbs (440.0, 1e-9));
    CHECK_THAT (StepMath::toHz (440.0, 1, 0.0), WithinAbs (880.0, 1e-9));
    CHECK_THAT (StepMath::toHz (440.0, -1, 0.0), WithinAbs (220.0, 1e-9));
    CHECK_THAT (StepMath::toHz (440.0, 0, 300.0), WithinAbs (523.2511306, 1e-6));

    // Mesmo resultado do TuningEngine para 19 EDO, nota 75 (step 6): ~547,66 Hz.
    CHECK_THAT (StepMath::toHz (440.0, 0, StepMath::stepCents (6, 19)), WithinAbs (547.665, 0.001));
}

TEST_CASE ("Steps: numeros desenhados conforme a EDO")
{
    CHECK (StepMath::labelStride (12) == 1);
    CHECK (StepMath::labelStride (24) == 1);
    CHECK (StepMath::labelStride (25) == 2);
    CHECK (StepMath::labelStride (48) == 2);
    CHECK (StepMath::labelStride (49) == 4);
    CHECK (StepMath::labelStride (72) == 4);

    CHECK (StepMath::shouldLabel (0, 72));
    CHECK_FALSE (StepMath::shouldLabel (2, 72));
    CHECK (StepMath::shouldLabel (4, 72));
    CHECK (StepMath::shouldLabel (7, 19));
}

TEST_CASE ("Steps: angulo e fracao da oitava")
{
    CHECK_THAT (StepMath::centsToAngle (0.0), WithinAbs (0.0, 1e-12));
    CHECK_THAT (StepMath::centsToAngle (300.0), WithinAbs (1.5707963268, 1e-9)); // 90 graus
    CHECK_THAT (StepMath::centsToAngle (600.0), WithinAbs (3.1415926536, 1e-9));
    CHECK_THAT (StepMath::centsToFraction (600.0), WithinAbs (0.5, 1e-12));
    CHECK_THAT (StepMath::centsToFraction (1200.0), WithinAbs (1.0, 1e-12));
}

TEST_CASE ("Steps: brilho da linha acesa")
{
    // Segurada: sempre 1.
    CHECK (StepMath::nextGlow (0.0f, true, 1.0 / 30.0, 0.3) == 1.0f);

    // Solta: some em ~0,3 s (9 quadros a 30 Hz) e nunca fica negativo.
    float glow = 1.0f;
    int frames = 0;
    while (glow > 0.0f && frames < 100)
    {
        glow = StepMath::nextGlow (glow, false, 1.0 / 30.0, 0.3);
        ++frames;
    }
    CHECK (glow == 0.0f);
    CHECK (frames >= 9);
    CHECK (frames <= 10);
}
