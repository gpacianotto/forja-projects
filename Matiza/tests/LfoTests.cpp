// Testes do LFO (RF11).
// Nomes de TEST_CASE sem acento: o ctest no Windows não os encontra se tiverem.
#include "Lfo.h"

#include <algorithm>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE ("LFO: senoide comeca em 0 e tem pico em 1/4 do ciclo")
{
    CHECK_THAT (Lfo::valueAt (LfoShape::sine, 0.0, 0, 0),  WithinAbs (0.0, 1e-6));
    CHECK_THAT (Lfo::valueAt (LfoShape::sine, 0.25, 0, 0), WithinAbs (1.0, 1e-6));
    CHECK_THAT (Lfo::valueAt (LfoShape::sine, 0.75, 0, 0), WithinAbs (-1.0, 1e-6));
}

TEST_CASE ("LFO: triangulo comeca em 0, sobe ate 1 e desce ate -1")
{
    CHECK_THAT (Lfo::valueAt (LfoShape::triangle, 0.0, 0, 0),   WithinAbs (0.0, 1e-6));
    CHECK_THAT (Lfo::valueAt (LfoShape::triangle, 0.25, 0, 0),  WithinAbs (1.0, 1e-6));
    CHECK_THAT (Lfo::valueAt (LfoShape::triangle, 0.5, 0, 0),   WithinAbs (0.0, 1e-6));
    CHECK_THAT (Lfo::valueAt (LfoShape::triangle, 0.75, 0, 0),  WithinAbs (-1.0, 1e-6));
    CHECK_THAT (Lfo::valueAt (LfoShape::triangle, 0.999, 0, 0), WithinAbs (0.0, 0.01));
}

TEST_CASE ("LFO: serras e quadrada")
{
    CHECK_THAT (Lfo::valueAt (LfoShape::sawUp, 0.0, 0, 0),   WithinAbs (-1.0, 1e-6));
    CHECK_THAT (Lfo::valueAt (LfoShape::sawUp, 0.5, 0, 0),   WithinAbs (0.0, 1e-6));
    CHECK_THAT (Lfo::valueAt (LfoShape::sawDown, 0.0, 0, 0), WithinAbs (1.0, 1e-6));
    CHECK (Lfo::valueAt (LfoShape::square, 0.2, 0, 0) == 1.0f);
    CHECK (Lfo::valueAt (LfoShape::square, 0.7, 0, 0) == -1.0f);
}

TEST_CASE ("LFO: sample and hold fica parado dentro do ciclo e muda no proximo")
{
    const float a = Lfo::valueAt (LfoShape::sampleHold, 0.1, 5, 42);
    const float b = Lfo::valueAt (LfoShape::sampleHold, 0.9, 5, 42);
    const float c = Lfo::valueAt (LfoShape::sampleHold, 0.1, 6, 42);

    CHECK (a == b);
    CHECK (a != c);
    CHECK (a >= -1.0f);
    CHECK (a <= 1.0f);
}

TEST_CASE ("LFO: sample and hold cobre a faixa toda e e deterministico")
{
    double minimum = 1.0, maximum = -1.0, sum = 0.0;
    for (int cycle = 0; cycle < 2000; ++cycle)
    {
        const double v = Lfo::randomBipolar (7, cycle);
        minimum = std::min (minimum, v);
        maximum = std::max (maximum, v);
        sum += v;
    }

    CHECK (minimum < -0.95);
    CHECK (maximum > 0.95);
    CHECK_THAT (sum / 2000.0, WithinAbs (0.0, 0.1)); // média perto de 0
    CHECK (Lfo::randomBipolar (7, 123) == Lfo::randomBipolar (7, 123));
    CHECK (Lfo::randomBipolar (7, 123) != Lfo::randomBipolar (8, 123));
}

TEST_CASE ("LFO: advance conta ciclos e mantem a fase entre 0 e 1")
{
    Lfo lfo;
    lfo.reset();

    lfo.advance (0.6);
    CHECK_THAT (lfo.getPhase(), WithinAbs (0.6, 1e-12));
    CHECK (lfo.getCycle() == 0);

    lfo.advance (0.6);
    CHECK_THAT (lfo.getPhase(), WithinAbs (0.2, 1e-12));
    CHECK (lfo.getCycle() == 1);

    lfo.advance (2.5); // vários ciclos de uma vez
    CHECK_THAT (lfo.getPhase(), WithinAbs (0.7, 1e-12));
    CHECK (lfo.getCycle() == 3);
}

TEST_CASE ("LFO: 1 Hz a 48 kHz completa um ciclo em 48000 amostras")
{
    Lfo lfo;
    lfo.reset();

    for (int i = 0; i < 48000; ++i)
        lfo.advance (1.0 / 48000.0);

    CHECK (lfo.getCycle() == 1);
    CHECK_THAT (lfo.getPhase(), WithinAbs (0.0, 1e-9));
}

TEST_CASE ("LFO: setPosition aceita fase maior que 1")
{
    Lfo lfo;
    lfo.setPosition (2.25, 10);
    CHECK_THAT (lfo.getPhase(), WithinAbs (0.25, 1e-12));
    CHECK (lfo.getCycle() == 12);
}

TEST_CASE ("LFO: frequencia sincronizada ao tempo")
{
    // 120 BPM = 2 semínimas por segundo.
    CHECK_THAT (Lfo::syncedHz (120.0, 1.0), WithinAbs (2.0, 1e-12)); // 1/4
    CHECK_THAT (Lfo::syncedHz (120.0, 4.0), WithinAbs (0.5, 1e-12)); // 1 compasso
    CHECK_THAT (Lfo::syncedHz (120.0, 0.5), WithinAbs (4.0, 1e-12)); // 1/8
    CHECK_THAT (Lfo::syncedHz (90.0, 1.0 / 3.0), WithinAbs (4.5, 1e-12)); // 1/8T
}

TEST_CASE ("LFO: listas fechadas nao mudam de ordem")
{
    CHECK (static_cast<int> (LfoShape::sampleHold) == 5);
    CHECK (lfoShapeNames.size() == 7); // + Custom (RF15)
    CHECK (lfoDivisionNames.size() == lfoDivisionBeats.size());
    CHECK (std::string (lfoDivisionNames[lfoDefaultDivision]) == "1/4");
    CHECK (lfoDivisionBeats[lfoDefaultDivision] == 1.0);
}
