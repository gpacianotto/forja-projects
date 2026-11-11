#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// Marcador exigido pelo juce::Synthesiser: diz quais notas e canais uma voz pode tocar.
// O Matiza aceita todas as notas em todos os canais.
struct MicroSound final : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};
