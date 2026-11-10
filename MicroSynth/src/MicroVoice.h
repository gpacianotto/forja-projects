#pragma once

#include <array>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "Glide.h"
#include "Oscillator.h"
#include "Parameters.h"
#include "TuningEngine.h"

// Configuração de um oscilador (RF6).
struct OscillatorParameters
{
    bool     enabled   = false;
    Waveform waveform  = Waveform::saw;
    float    level     = 1.0f;  // 0..1
    int      octave    = 0;
    int      steps     = 0;     // passos da EDO
    float    fineCents = 0.0f;
};

// Valores dos parâmetros que as vozes usam. O processador preenche uma vez por bloco
// e repassa a todas as vozes (struct simples, copiada sem alocar).
struct VoiceParameters
{
    std::array<OscillatorParameters, ParamIDs::numOscillators> oscillators {};
    juce::ADSR::Parameters adsr { 0.005f, 0.2f, 0.8f, 0.3f }; // em segundos
    float cutoffHz     = 8000.0f;
    float resonance    = 0.7f;
    float glideSeconds = 0.0f;
};

// Uma voz do synth: 3 osciladores somados -> filtro passa-baixa -> envelope de amplitude (ADSR).
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

    // Modo Mono (RF7): o MicroSynthesiser avisa, logo antes de trocar a nota desta voz, que é
    // uma troca de nota e não uma nota nova. Assim a voz ignora o "corte" que o
    // juce::Synthesiser faz antes de cada nota e desliza (glide) em vez de recomeçar do zero.
    // retriggerEnvelope = false é o legato: o envelope segue sem redisparar.
    void prepareMonoHandoff (bool retriggerEnvelope) noexcept
    {
        monoHandoffPending = true;
        monoRetrigger      = retriggerEnvelope;
    }

    bool canPlaySound (juce::SynthesiserSound*) override;
    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override;

private:
    void updateFrequencies();
    float clampCutoff (float hz) const;

    const TuningEngine& tuning;
    VoiceParameters parameters;

    // Cópia da afinação tirada no note-on: se edo/baseHz mudarem com a nota presa, ela não
    // muda (RF3), nem quando o pitch bend recalcula a frequência.
    TuningEngine noteTuning;
    double bend = 0.0; // -1..+1

    // Altura atual em notas MIDI/passos (fracionária durante o glide).
    Glide glide;

    std::array<Oscillator, ParamIDs::numOscillators> oscillators;

    // Volume de cada oscilador, suavizado (20 ms): ligar, desligar ou girar o knob não estala.
    // O alvo é 0 quando o oscilador está desligado ou fora da faixa tocável (RF4).
    std::array<juce::SmoothedValue<float>, ParamIDs::numOscillators> oscLevels;

    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::ADSR adsr;

    // Suaviza o cutoff (20 ms) para não estalar ao girar o knob ou automatizar.
    // Multiplicativo: anda em proporção (oitavas), como o ouvido percebe frequência.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoffSmoothed;

    float level = 0.0f; // volume da nota (vem da velocity)

    bool monoHandoffPending = false;
    bool monoRetrigger      = true;
};
