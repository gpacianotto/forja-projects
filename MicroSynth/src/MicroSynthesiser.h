#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "MicroVoice.h"
#include "NoteStack.h"
#include "TuningEngine.h"

// juce::Synthesiser com duas extensões:
//
// 1. Guarda do RF4: notas cuja frequência fique fora da faixa tocável (abaixo de 20 Hz ou
//    acima de 45% da taxa de amostragem) são ignoradas. O filtro fica aqui, antes do
//    Synthesiser::noteOn, para que uma nota ignorada não ocupe nem roube uma voz.
//
// 2. Modo Mono (RF7): só a voz 0 toca. As teclas pressionadas ficam numa pilha; a última
//    pressionada é a que soa, e ao soltá-la o synth volta para a anterior.
class MicroSynthesiser final : public juce::Synthesiser
{
public:
    explicit MicroSynthesiser (const TuningEngine& tuningEngine) : tuning (tuningEngine) {}

    // Chamado no início de cada processBlock (thread de áudio). Trocar de modo solta
    // todas as notas, com release, para não deixar notas presas.
    void setVoiceMode (bool mono, bool legatoEnabled)
    {
        if (mono != monoMode)
        {
            allNotesOff (0, true);
            monoMode = mono;
        }

        legato = legatoEnabled;
    }

    void noteOn (int midiChannel, int midiNoteNumber, float velocity) override
    {
        // Frequência sem pitch bend: o bend é um desvio momentâneo, não decide se a nota existe.
        if (! TuningEngine::isPlayable (tuning.noteToHz (midiNoteNumber), getSampleRate()))
            return;

        if (! monoMode)
        {
            juce::Synthesiser::noteOn (midiChannel, midiNoteNumber, velocity);
            return;
        }

        // Legato só quando outra tecla ainda está pressionada (tocar "ligado").
        const bool anotherKeyHeld = ! heldNotes.empty();
        heldNotes.push ({ midiNoteNumber, midiChannel, velocity });
        playMonoNote (midiChannel, midiNoteNumber, velocity, ! (legato && anotherKeyHeld));
    }

    void noteOff (int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff) override
    {
        if (! monoMode)
        {
            juce::Synthesiser::noteOff (midiChannel, midiNoteNumber, velocity, allowTailOff);
            return;
        }

        auto* voice = getVoice (0);
        const bool isSoundingNote = voice != nullptr
                                 && voice->getCurrentlyPlayingNote() == midiNoteNumber
                                 && voice->isPlayingChannel (midiChannel);

        heldNotes.remove (midiNoteNumber, midiChannel);

        // Soltou uma tecla que não é a que está soando: só sai da pilha.
        if (! isSoundingNote)
            return;

        // Ainda há teclas pressionadas: volta para a mais recente.
        if (! heldNotes.empty())
        {
            const auto previous = heldNotes.top();
            playMonoNote (previous.channel, previous.note, previous.velocity, ! legato);
            return;
        }

        // Nenhuma tecla: release normal (o juce::Synthesiser cuida do pedal de sustain).
        juce::Synthesiser::noteOff (midiChannel, midiNoteNumber, velocity, allowTailOff);
    }

    void allNotesOff (int midiChannel, bool allowTailOff) override
    {
        heldNotes.clear();
        juce::Synthesiser::allNotesOff (midiChannel, allowTailOff);
    }

private:
    void playMonoNote (int midiChannel, int midiNoteNumber, float velocity, bool retriggerEnvelope)
    {
        auto* voice = dynamic_cast<MicroVoice*> (getVoice (0));
        auto sound  = getSound (0);

        if (voice == nullptr || sound == nullptr)
            return;

        // Avisa a voz que é uma troca de nota; startVoice (do juce::Synthesiser) atualiza o
        // registro de qual nota a voz toca e chama stopNote + startNote nela.
        voice->prepareMonoHandoff (retriggerEnvelope);
        startVoice (voice, sound.get(), midiChannel, midiNoteNumber, velocity);
    }

    const TuningEngine& tuning;

    NoteStack heldNotes;
    bool monoMode = false;
    bool legato   = true;
};
