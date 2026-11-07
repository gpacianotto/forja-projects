// Nomes de TEST_CASE sem acento: o ctest no Windows não os encontra se tiverem.
#include "TuningEngine.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace
{
    TuningEngine makeEngine (int edo, double baseHz = 440.0, int refNote = 69, double bendSteps = 2.0)
    {
        TuningEngine engine;
        engine.setSettings ({ edo, baseHz, refNote, bendSteps });
        return engine;
    }
}

TEST_CASE ("12 EDO, 440 Hz, ref 69 = temperamento padrao (RF2)")
{
    const auto engine = makeEngine (12);

    // Mesma fórmula de juce::MidiMessage::getMidiNoteInHertz, escrita aqui para o teste
    // não depender do JUCE.
    for (int note = 0; note <= 127; ++note)
    {
        const double expected = 440.0 * std::pow (2.0, (note - 69) / 12.0);
        CHECK_THAT (engine.noteToHz (note), WithinAbs (expected, 0.01));
    }

    // Valores de referência conhecidos
    CHECK_THAT (engine.noteToHz (60), WithinAbs (261.63, 0.01)); // dó central
    CHECK_THAT (engine.noteToHz (0),  WithinAbs (8.18, 0.01));
    CHECK_THAT (engine.noteToHz (127), WithinAbs (12543.85, 0.01));
}

TEST_CASE ("Tabela de exemplos do Spec (24 EDO)")
{
    const auto engine = makeEngine (24);

    CHECK_THAT (engine.noteToHz (69), WithinAbs (440.00, 0.01));
    CHECK_THAT (engine.noteToHz (70), WithinAbs (452.89, 0.01));
    CHECK_THAT (engine.noteToHz (71), WithinAbs (466.16, 0.01));
    CHECK_THAT (engine.noteToHz (72), WithinAbs (479.82, 0.01));
}

TEST_CASE ("Subir N passos em N-EDO dobra a frequencia")
{
    for (int edo : { 12, 19, 24, 31, 52 })
    {
        INFO ("edo = " << edo);
        const auto engine = makeEngine (edo);

        for (int note : { 10, 40, 69 })
            CHECK_THAT (engine.noteToHz (note + edo), WithinRel (2.0 * engine.noteToHz (note), 1e-12));
    }
}

TEST_CASE ("A nota de referencia sempre devolve exatamente baseHz")
{
    for (int edo : { 1, 12, 24, 31, 72 })
        for (double baseHz : { 20.0, 261.63, 432.0, 2000.0 })
            for (int refNote : { 0, 60, 69, 127 })
                CHECK (makeEngine (edo, baseHz, refNote).noteToHz (refNote) == baseHz);
}

TEST_CASE ("Pitch bend maximo com bendSteps = 2 = dois passos acima")
{
    for (int edo : { 12, 24, 31 })
    {
        INFO ("edo = " << edo);
        const auto engine = makeEngine (edo, 440.0, 69, 2.0);

        CHECK_THAT (engine.noteToHz (60, +1.0), WithinRel (engine.noteToHz (62), 1e-12));
        CHECK_THAT (engine.noteToHz (60, -1.0), WithinRel (engine.noteToHz (58), 1e-12));
        CHECK (engine.noteToHz (60, 0.0) == engine.noteToHz (60));
    }
}

TEST_CASE ("edo invalido (0) e corrigido para 1, sem divisao por zero")
{
    const auto engine = makeEngine (0);

    CHECK (engine.getSettings().edo == 1);
    CHECK_THAT (engine.noteToHz (70), WithinRel (880.0, 1e-12));
}
