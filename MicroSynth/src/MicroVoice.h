#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// Uma voz do synth: oscilador + envelope de amplitude (ADSR).
// O juce::Synthesiser tem várias vozes e entrega cada nota nova a uma voz livre.
class MicroVoice final : public juce::SynthesiserVoice
{
public:
    // Chamado em prepareToPlay, fora da thread de áudio.
    void prepare (double sampleRate);

    bool canPlaySound (juce::SynthesiserSound*) override;
    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override;

private:
    juce::ADSR adsr;

    double phase = 0.0;          // posição no ciclo da onda, de 0 a 1
    double phaseIncrement = 0.0; // quanto a fase avança por amostra (= frequência / sample rate)
    float level = 0.0f;          // volume da nota (vem da velocity)
};
