#pragma once

#include <array>
#include <atomic>
#include <cstddef>

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
// O objeto é grande (~1,6 MB por causa dos mipmaps): o processador o cria no heap.
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

    // --- Thread de áudio (uma chamada por bloco para cada um) ---
    // LFO: Lfo::customTableSize + 1 amostras. Oscilador: Wavetables::mipmapSetSize amostras.
    const float* readLfoTable (std::size_t lfo) noexcept { return lfoTables[lfo].read().data(); }
    const float* readOscTables (std::size_t osc) noexcept { return oscTables[osc].read().data(); }

private:
    using LfoTable = std::array<float, Lfo::customTableSize + 1>;
    using OscTables = std::array<float, Wavetables::mipmapSetSize>;

    static void renderLfo (const ShapePoints& points, LfoTable& out);
    static void renderOsc (const ShapePoints& points, OscTables& out);

    std::array<ShapePoints, numLfos> lfoPoints;
    std::array<ShapePoints, numOscillators> oscPoints;

    std::array<TripleBuffer<LfoTable>, numLfos> lfoTables;
    std::array<TripleBuffer<OscTables>, numOscillators> oscTables;
};
