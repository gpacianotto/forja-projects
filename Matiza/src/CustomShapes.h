#pragma once

#include <array>
#include <atomic>
#include <cstddef>

#include "AlignedSpectrum.h"
#include "Lfo.h"
#include "ShapePoints.h"
#include "TripleBuffer.h"
#include "Wavetables.h"

// Formas desenhadas dos 3 LFOs e dos 3 osciladores (RF15). C++ puro, sem JUCE.
//
// Dois lados:
// - Thread da interface: lê e muda os pontos. Cada mudança monta a tabela de áudio na hora
//   (LFO: 2049 amostras; oscilador: os 21 mipmaps, com FFT) e a publica.
// - Thread de áudio: no começo de cada bloco, pega a tabela publicada mais recente (read*).
//   Sem lock e sem alocação: as tabelas vivem em buffers triplos (TripleBuffer.h).
//
// O objeto é grande (~3 MB por causa dos mipmaps): o processador o cria no heap.
class CustomShapes
{
public:
    static constexpr std::size_t numLfos = 3;
    static constexpr std::size_t numOscillators = 3;

    CustomShapes();

    // --- Thread da interface ---
    const ShapePoints& getLfoPoints (std::size_t lfo) const noexcept { return lfoPoints[lfo]; }
    const ShapePoints& getOscPoints (std::size_t osc) const noexcept { return oscPoints[osc]; }
    void setLfoPoints (std::size_t lfo, const ShapePoints& points);
    void setOscPoints (std::size_t osc, const ShapePoints& points);

    // Tudo o que a voz precisa de uma onda desenhada: os mipmaps completos, os "do resto" e a
    // receita dos 24 primeiros harmônicos (RF21). Montados juntos, publicados juntos: a voz
    // nunca mistura a receita de uma versão do desenho com as tabelas de outra.
    struct OscShape
    {
        std::array<float, Wavetables::mipmapSetSize> tables;   // Wavetables::mipmapSetSize amostras
        std::array<float, Wavetables::mipmapSetSize> residual; // sem os harmônicos 1..24
        AlignedSpectrum::Recipe recipe;
    };

    // --- Thread de áudio (uma chamada por bloco para cada um) ---
    // LFO: Lfo::customTableSize + 1 amostras. A referência vale até o próximo read do mesmo item.
    const float* readLfoTable (std::size_t lfo) noexcept { return lfoTables[lfo].read().data(); }
    const OscShape& readOscShape (std::size_t osc) noexcept { return oscShapes[osc].read(); }

private:
    using LfoTable = std::array<float, Lfo::customTableSize + 1>;

    static void renderLfo (const ShapePoints& points, LfoTable& out);
    static void renderOsc (const ShapePoints& points, OscShape& out);

    std::array<ShapePoints, numLfos> lfoPoints;
    std::array<ShapePoints, numOscillators> oscPoints;

    std::array<TripleBuffer<LfoTable>, numLfos> lfoTables;
    std::array<TripleBuffer<OscShape>, numOscillators> oscShapes;
};
