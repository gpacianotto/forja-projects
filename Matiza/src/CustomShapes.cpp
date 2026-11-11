#include "CustomShapes.h"

#include <vector>

CustomShapes::CustomShapes()
{
    // Pontos iniciais (antes de o usuário desenhar): seno nos LFOs e serra nos osciladores,
    // as formas padrão de cada um.
    for (std::size_t k = 0; k < numLfos; ++k)
    {
        lfoPoints[k] = ShapePoints::preset (ShapePoints::Preset::sine);
        lfoTables[k].fillAll ([this, k] (LfoTable& table) { renderLfo (lfoPoints[k], table); });
    }

    for (std::size_t i = 0; i < numOscillators; ++i)
    {
        oscPoints[i] = ShapePoints::preset (ShapePoints::Preset::sawUp);
        oscTables[i].fillAll ([this, i] (OscTables& tables) { renderOsc (oscPoints[i], tables); });
    }
}

void CustomShapes::setLfoPoints (std::size_t lfo, const ShapePoints& points)
{
    lfoPoints[lfo] = points;
    renderLfo (points, lfoTables[lfo].beginWrite());
    lfoTables[lfo].publish();
}

void CustomShapes::setOscPoints (std::size_t osc, const ShapePoints& points)
{
    oscPoints[osc] = points;
    renderOsc (points, oscTables[osc].beginWrite());
    oscTables[osc].publish();
}

void CustomShapes::renderLfo (const ShapePoints& points, LfoTable& out)
{
    // Amostra i = posição i / 2048 do ciclo; a última (2048) é o fim do ciclo (x = 1), que
    // pode ser diferente do começo (ex.: uma rampa).
    for (std::size_t i = 0; i < out.size(); ++i)
        out[i] = points.evaluate (static_cast<double> (i) / Lfo::customTableSize);
}

void CustomShapes::renderOsc (const ShapePoints& points, OscTables& out)
{
    std::vector<float> cycle (Wavetables::tableSize);
    for (int i = 0; i < Wavetables::tableSize; ++i)
        cycle[static_cast<std::size_t> (i)] = points.evaluate (static_cast<double> (i) / Wavetables::tableSize);

    Wavetables::buildMipmaps (cycle.data(), out.data());
}
