#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "Oscillator.h"
#include "TuningEngine.h"

// Valores dos parâmetros que as vozes usam. O processador preenche uma vez por bloco
// e repassa a todas as vozes (struct simples, copiada sem alocar).
struct VoiceParameters
{
    Waveform waveform = Waveform::saw;
    juce::ADSR::Parameters adsr { 0.005f, 0.2f, 0.8f, 0.3f }; // em segundos
    float cutoffHz  = 8000.0f;
    float resonance = 0.7f;
};

// Uma voz do synth: oscilador -> filtro passa-baixa -> envelope de amplitude (ADSR).
// O juce::Synthesiser tem várias vozes e entrega cada nota nova a uma voz livre.
class MicroVoice final : public juce::SynthesiserVoice
{
public:
    // A voz só lê o TuningEngine (const&); o dono é o PluginProcessor, que vive mais que ela.
    explicit MicroVoice (const TuningEngine& tuningEngine) : tuning (tuningEngine) {}

    // Chamado em prepareToPlay, fora da thread de áudio (o filtro aloca memória aqui).
    void prepare (double sampleRate, int maxBlockSize);

    // Chamado a cada bloco, na thread de áudio. Não aloca.
    void setParameters (const VoiceParameters& params);

    bool canPlaySound (juce::SynthesiserSound*) override;
    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override;

private:
    void updateFrequency();
    float clampCutoff (float hz) const;

    const TuningEngine& tuning;

    // Cópia da afinação tirada no note-on: se edo/baseHz mudarem com a nota presa, ela não
    // muda (RF3), nem quando o pitch bend recalcula a frequência.
    TuningEngine noteTuning;
    int currentNote = 0;
    double bend = 0.0; // -1..+1

    Oscillator oscillator;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::ADSR adsr;

    // Suaviza o cutoff (20 ms) para não estalar ao girar o knob ou automatizar.
    // Multiplicativo: anda em proporção (oitavas), como o ouvido percebe frequência.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoffSmoothed;

    float level = 0.0f; // volume da nota (vem da velocity)
};
