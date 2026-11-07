#include "MicroVoice.h"
#include "MicroSound.h"

void MicroVoice::prepare (double sampleRate)
{
    adsr.setSampleRate (sampleRate);

    // Etapa 2: envelope fixo com os padrões do Spec. Vira parâmetro na etapa 4.
    juce::ADSR::Parameters params;
    params.attack  = 0.005f; // 5 ms
    params.decay   = 0.2f;   // 200 ms
    params.sustain = 0.8f;
    params.release = 0.3f;   // 300 ms
    adsr.setParameters (params);
}

bool MicroVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<MicroSound*> (sound) != nullptr;
}

void MicroVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int)
{
    // TEMPORÁRIO (etapa 2): afinação padrão de 12 tons do JUCE, como pede o Spec.
    // Na etapa 3 isto vira TuningEngine::noteToHz.
    const double frequency = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber);

    phase = 0.0;
    phaseIncrement = frequency / getSampleRate();
    level = 0.25f * velocity; // margem para 8 vozes somadas não saturarem

    adsr.noteOn();
}

void MicroVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        adsr.noteOff(); // entra no release; a voz é liberada em renderNextBlock quando acabar
    }
    else
    {
        // O host pediu silêncio imediato (ex.: roubo de voz ou "all notes off").
        adsr.reset();
        clearCurrentNote();
    }
}

void MicroVoice::renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    const int numChannels = output.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        const float envelope = adsr.getNextSample();
        const float sample   = std::sin (juce::MathConstants<float>::twoPi * static_cast<float> (phase))
                               * level * envelope;

        phase += phaseIncrement;
        if (phase >= 1.0)
            phase -= 1.0;

        // Somar, não sobrescrever: as outras vozes escrevem no mesmo buffer.
        for (int ch = 0; ch < numChannels; ++ch)
            output.addSample (ch, startSample + i, sample);

        if (! adsr.isActive())
        {
            clearCurrentNote(); // fim do release: a voz fica livre para outra nota
            break;
        }
    }
}
