#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "TuningEngine.h"

// juce::Synthesiser com a guarda do RF4: notas cuja frequência fique fora da faixa tocável
// (abaixo de 20 Hz ou acima de 45% da taxa de amostragem) são ignoradas.
//
// O filtro fica aqui, antes do Synthesiser::noteOn, para que uma nota ignorada não ocupe
// nem roube uma voz que está tocando.
class MicroSynthesiser final : public juce::Synthesiser
{
public:
    explicit MicroSynthesiser (const TuningEngine& tuningEngine) : tuning (tuningEngine) {}

    void noteOn (int midiChannel, int midiNoteNumber, float velocity) override
    {
        // Frequência sem pitch bend: o bend é um desvio momentâneo, não decide se a nota existe.
        if (! TuningEngine::isPlayable (tuning.noteToHz (midiNoteNumber), getSampleRate()))
            return;

        juce::Synthesiser::noteOn (midiChannel, midiNoteNumber, velocity);
    }

private:
    const TuningEngine& tuning;
};
