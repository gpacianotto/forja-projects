#pragma once

#include <array>
#include <atomic>
#include <optional>

#include "ModMatrix.h"

// Modulação animada (RF20): quanto cada destino da matriz está deslocado agora, na última voz
// tocada. A thread de áudio escreve uma vez por bloco; a interface lê a 30 Hz.
//
// São atômicos separados (um por destino), como o VoicePitchDisplay: ler um destino "novo" e
// outro "velho" no mesmo quadro só desenharia um quadro levemente fora de sincronia, sem risco.
struct ModulationDisplay
{
    std::atomic<bool> active { false }; // false = nenhuma voz tocando (nem no release)
    std::array<std::atomic<float>, ModMatrix::numDests> offsets {};

    // Deslocamento atual de um destino (unidades normalizadas, como ModMatrix::computeOffsets),
    // ou nada se nenhuma voz estiver tocando. Só na thread da interface.
    std::optional<float> read (ModMatrix::Dest dest) const
    {
        if (! active.load())
            return std::nullopt;

        return offsets[ModMatrix::index (dest)].load();
    }
};
