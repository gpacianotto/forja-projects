#include "MicroVoice.h"
#include "MicroSound.h"

void MicroVoice::prepare (double sampleRate, int maxBlockSize)
{
    adsr.setSampleRate (sampleRate);

    // Uma voz gera um sinal mono; ele é copiado para os canais no renderNextBlock.
    filter.prepare ({ sampleRate, static_cast<juce::uint32> (maxBlockSize), 1 });
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    cutoffSmoothed.reset (sampleRate, 0.02);
    cutoffSmoothed.setCurrentAndTargetValue (clampCutoff (8000.0f));
    filter.setCutoffFrequency (cutoffSmoothed.getCurrentValue());
}

float MicroVoice::clampCutoff (float hz) const
{
    // O filtro exige cutoff abaixo de Nyquist (metade da taxa de amostragem).
    const auto maxHz = static_cast<float> (getSampleRate() * 0.45);
    return juce::jlimit (20.0f, juce::jmax (20.0f, maxHz), hz);
}

void MicroVoice::setParameters (const VoiceParameters& params)
{
    oscillator.setWaveform (params.waveform);
    adsr.setParameters (params.adsr);
    filter.setResonance (params.resonance);
    cutoffSmoothed.setTargetValue (clampCutoff (params.cutoffHz));
}

bool MicroVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<MicroSound*> (sound) != nullptr;
}

void MicroVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*,
                            int currentPitchWheelPosition)
{
    noteTuning  = tuning; // cópia (sem alocação: só números)
    currentNote = midiNoteNumber;
    bend        = TuningEngine::pitchWheelToBend (currentPitchWheelPosition);

    oscillator.resetPhase();
    updateFrequency();

    // Nota nova começa com o filtro "limpo" e já no cutoff atual, sem deslizar.
    filter.reset();
    cutoffSmoothed.setCurrentAndTargetValue (cutoffSmoothed.getTargetValue());
    filter.setCutoffFrequency (cutoffSmoothed.getCurrentValue());

    level = velocity;
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

void MicroVoice::pitchWheelMoved (int newPitchWheelValue)
{
    bend = TuningEngine::pitchWheelToBend (newPitchWheelValue);
    updateFrequency();
}

void MicroVoice::updateFrequency()
{
    // RF1: a frequência vem do motor de afinação (qualquer EDO), nunca do 12 tons do JUCE.
    oscillator.setFrequency (noteTuning.noteToHz (currentNote, bend), getSampleRate());
}

void MicroVoice::renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    const int numChannels = output.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        if (cutoffSmoothed.isSmoothing())
            filter.setCutoffFrequency (cutoffSmoothed.getNextValue());

        const float raw      = oscillator.nextSample();
        const float filtered = filter.processSample (0, raw);
        const float sample   = filtered * adsr.getNextSample() * level;

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
