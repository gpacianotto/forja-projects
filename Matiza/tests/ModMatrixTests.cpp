// Testes da matriz de modulação (RF10).
// Nomes de TEST_CASE sem acento: o ctest no Windows não os encontra se tiverem.
#include "ModMatrix.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using namespace ModMatrix;

namespace
{
    SourceValues makeSources()
    {
        SourceValues values {};
        values[index (Source::velocity)] = 0.5f;
        values[index (Source::modEnv)]   = 1.0f;
        values[index (Source::keyTrack)] = -1.0f;
        return values;
    }
}

TEST_CASE ("Matriz: sem slots ativos, nenhum deslocamento")
{
    Slots slots {};
    DestOffsets offsets;
    offsets.fill (9.0f); // lixo: precisa ser zerado

    computeOffsets (slots, makeSources(), offsets);

    for (float offset : offsets)
        CHECK (offset == 0.0f);
}

TEST_CASE ("Matriz: deslocamento = valor da fonte x quantidade")
{
    Slots slots {};
    slots[0] = { Source::velocity, Dest::cutoff, 0.8f };

    DestOffsets offsets;
    computeOffsets (slots, makeSources(), offsets);

    CHECK_THAT (offsets[index (Dest::cutoff)], WithinAbs (0.4, 1e-6));
    CHECK (offsets[index (Dest::resonance)] == 0.0f);
}

TEST_CASE ("Matriz: varios slots no mesmo destino se somam")
{
    Slots slots {};
    slots[0] = { Source::velocity, Dest::cutoff, 0.8f };   // +0.4
    slots[3] = { Source::modEnv,   Dest::cutoff, -0.1f };  // -0.1
    slots[7] = { Source::keyTrack, Dest::cutoff, 0.5f };   // -0.5

    DestOffsets offsets;
    computeOffsets (slots, makeSources(), offsets);

    CHECK_THAT (offsets[index (Dest::cutoff)], WithinAbs (-0.2, 1e-6));
}

TEST_CASE ("Matriz: slot com fonte ou destino None e ignorado")
{
    Slots slots {};
    slots[0] = { Source::none,   Dest::cutoff, 1.0f };
    slots[1] = { Source::modEnv, Dest::none,   1.0f };

    DestOffsets offsets;
    computeOffsets (slots, makeSources(), offsets);

    CHECK (offsets[index (Dest::cutoff)] == 0.0f);
    CHECK (offsets[index (Dest::none)] == 0.0f);
}

TEST_CASE ("Matriz: applyNormalized limita entre 0 e 1")
{
    CHECK_THAT (applyNormalized (0.5f, 0.25f), WithinAbs (0.75, 1e-6));
    CHECK (applyNormalized (0.9f, 0.5f) == 1.0f);
    CHECK (applyNormalized (0.1f, -0.5f) == 0.0f);
}

TEST_CASE ("Matriz: key track vai de -1 a +1")
{
    CHECK (keyTrack (0) == -1.0f);
    CHECK (keyTrack (127) == 1.0f);
    CHECK_THAT (keyTrack (64), WithinAbs (0.5 / 63.5, 1e-6));
    CHECK (keyTrack (-5) == -1.0f); // fora da faixa MIDI: limitado
}

TEST_CASE ("Matriz: fontes bipolares e unipolares")
{
    CHECK (isBipolar (Source::keyTrack));
    CHECK (isBipolar (Source::lfo1));
    CHECK_FALSE (isBipolar (Source::velocity));
    CHECK_FALSE (isBipolar (Source::modEnv));
}

TEST_CASE ("Matriz: destinos de cada oscilador")
{
    CHECK (oscLevelDest (0) == Dest::osc1Level);
    CHECK (oscPitchDest (1) == Dest::osc2Pitch);
    CHECK (oscFineDest (2) == Dest::osc3Fine);
}

TEST_CASE ("Matriz: listas fechadas nao mudam de ordem")
{
    // Protege os sets salvos: se alguém reordenar os enums, este teste quebra.
    CHECK (index (Source::modEnv) == 5);
    CHECK (index (Source::lfo3) == 8);
    CHECK (numSources == 11);
    CHECK (index (Dest::pitch) == 3);
    CHECK (index (Dest::osc3Fine) == 12);
    CHECK (numDests == 17);
    CHECK (numUsableSources == 9);
    CHECK (numUsableDests == 13);
}
