#pragma once

#include <array>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include "Glide.h"
#include "GraphMath.h"
#include "Lfo.h"
#include "ModMatrix.h"
#include "Oscillator.h"
#include "Parameters.h"
#include "TuningEngine.h"
#include "Unison.h"

// Configuração de um oscilador (RF6).
struct OscillatorParameters
{
    bool     enabled   = false;
    Waveform waveform  = Waveform::saw;
    float    level     = 1.0f;  // 0..1
    int      octave    = 0;
    int      steps     = 0;     // passos da EDO
    float    fineCents = 0.0f;

    // Unison (RF12)
    int   unison      = 1;
    float detuneCents = 25.0f;
    float blend       = 0.5f; // 0..1
    float width       = 1.0f; // 0..1
};

// Configuração de um LFO (RF11). rateHz já vem calculado pelo processador (Hz livre ou
// convertido do tempo do host quando há Sync).
struct LfoParameters
{
    LfoShape shape     = LfoShape::sine;
    double   rateHz    = 2.0;
    bool     retrigger = true; // false = Free (fase comum a todas as vozes)
};

// Valores dos parâmetros que as vozes usam. O processador preenche uma vez por bloco
// e repassa a todas as vozes (struct simples, copiada sem alocar).
struct VoiceParameters
{
    std::array<OscillatorParameters, ParamIDs::numOscillators> oscillators {};
    juce::ADSR::Parameters adsr    { 0.005f, 0.2f, 0.8f, 0.3f }; // em segundos
    juce::ADSR::Parameters modEnv  { 0.005f, 0.3f, 0.0f, 0.3f }; // envelope de modulação (RF10)
    float cutoffHz     = 8000.0f;
    float resonance    = 0.7f;
    float glideSeconds = 0.0f;
    ModMatrix::Slots modSlots {};
    std::array<LfoParameters, ParamIDs::numLfos> lfos {};

    // Formas desenhadas (RF15), lidas pelo processador no começo do bloco. Válidas até o
    // próximo bloco.
    std::array<const float*, ParamIDs::numOscillators> oscCustomTables {};
    std::array<const float*, ParamIDs::numLfos> lfoCustomTables {};
};

// Faixas normalizadas dos destinos com escala não linear. A matriz soma em valores
// normalizados (0 a 1), como a posição do knob; estas faixas convertem de/para Hz e Q.
// O processador preenche uma vez no construtor; as vozes só leem.
struct ModRanges
{
    juce::NormalisableRange<float> cutoff;
    juce::NormalisableRange<float> resonance;
};

// Fontes de modulação globais (não dependem da nota). Escritas pelo MatizaSynthesiser ao
// receber MIDI e lidas pelas vozes, as duas coisas na thread de áudio.
struct GlobalModState
{
    float modWheel        = 0.0f; // CC 1, 0..1
    float channelPressure = 0.0f; // aftertouch de canal, 0..1

    // Posição dos LFOs no modo Free no início do bloco atual (escrita pelo processador).
    struct LfoPosition
    {
        double phase = 0.0;
        std::int64_t cycle = 0;
    };
    std::array<LfoPosition, ParamIDs::numLfos> lfoPositions {};
};

// Uma voz do synth: 3 osciladores (cada um com até 8 cópias de unison, RF12) somados em estéreo
// -> filtro passa-baixa -> envelope de amplitude (ADSR), com a matriz de modulação (RF10)
// recalculada a cada 32 amostras.
class MatizaVoice final : public juce::SynthesiserVoice
{
public:
    // A voz só lê estas referências; o dono é o PluginProcessor, que vive mais que ela.
    MatizaVoice (const TuningEngine& tuningEngine, const GlobalModState& globalModState, const ModRanges& modRanges)
        : tuning (tuningEngine), globalMod (globalModState), ranges (modRanges) {}

    // Chamado em prepareToPlay, fora da thread de áudio (o filtro aloca memória aqui).
    void prepare (double sampleRate, int maxBlockSize);

    // Chamado a cada bloco, na thread de áudio. Não aloca.
    void setParameters (const VoiceParameters& params);

    // Modo Mono (RF7): o MatizaSynthesiser avisa, logo antes de trocar a nota desta voz, que é
    // uma troca de nota e não uma nota nova. Assim a voz ignora o "corte" que o
    // juce::Synthesiser faz antes de cada nota e desliza (glide) em vez de recomeçar do zero.
    // retriggerEnvelope = false é o legato: o envelope segue sem redisparar.
    void prepareMonoHandoff (bool retriggerEnvelope) noexcept
    {
        monoHandoffPending = true;
        monoRetrigger      = retriggerEnvelope;
    }

    // Onde os envelopes desta voz estão, para o ponto dos gráficos (RF9 e RF10).
    struct EnvelopeSnapshot
    {
        GraphMath::EnvelopeStage stage = GraphMath::EnvelopeStage::idle;
        double secondsInStage = 0.0;
        float  level    = 0.0f; // envelope de volume, 0..1
        float  modLevel = 0.0f; // envelope de modulação, 0..1
    };
    EnvelopeSnapshot getEnvelopeSnapshot() const noexcept;

    // Fase atual de um LFO desta voz (para o ponto do editor no modo Retrigger, RF15).
    double getLfoPhase (std::size_t lfo) const noexcept { return lfos[lfo].getPhase(); }

    bool canPlaySound (juce::SynthesiserSound*) override;
    void startNote (int midiNoteNumber, float velocity,
                    juce::SynthesiserSound*, int currentPitchWheelPosition) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newPitchWheelValue) override;
    void controllerMoved (int, int) override {} // mod wheel é global (GlobalModState)
    void aftertouchChanged (int newAftertouchValue) override; // aftertouch polifônico, só desta nota
    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override;

private:
    void updateModulation();
    void restartLfos();
    void updateFrequencies();
    void updateUnisonLayouts();
    float clampCutoff (float hz) const;
    static void setEnvelopeParameters (juce::ADSR& envelope, const juce::ADSR::Parameters& newParams);

    const TuningEngine& tuning;
    const GlobalModState& globalMod;
    const ModRanges& ranges;
    VoiceParameters parameters;

    // Cópia da afinação tirada no note-on: se edo/baseHz mudarem com a nota presa, ela não
    // muda (RF3), nem quando o pitch bend recalcula a frequência.
    TuningEngine noteTuning;
    double bend = 0.0; // -1..+1
    int currentMidiNote = 60; // para o Key Track

    // Altura atual em notas MIDI/passos (fracionária durante o glide).
    Glide glide;

    // Cópias de unison de cada oscilador (RF12): oscillators[o][k] = cópia k do oscilador o.
    // Todas existem desde o início (sem alocação); só as primeiras `count` tocam.
    std::array<std::array<Oscillator, Unison::maxVoices>, ParamIDs::numOscillators> oscillators;
    std::array<Unison::Layout, ParamIDs::numOscillators> unisonLayouts {};

    // Cópia fora da faixa tocável (RF4) fica muda; as outras do mesmo oscilador continuam.
    std::array<std::array<bool, Unison::maxVoices>, ParamIDs::numOscillators> copyAudible {};

    // Blend e Width suavizados (20 ms): girar o knob muda o volume e o pan das cópias sem estalar.
    std::array<juce::SmoothedValue<float>, ParamIDs::numOscillators> blendSmoothed, widthSmoothed;

    // Volume de cada oscilador, suavizado (20 ms): ligar, desligar ou girar o knob não estala.
    // O alvo é 0 quando o oscilador está desligado ou fora da faixa tocável (RF4).
    std::array<juce::SmoothedValue<float>, ParamIDs::numOscillators> oscLevels;

    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::ADSR adsr;
    juce::ADSR modEnv; // envelope de modulação: não toca som, só alimenta a matriz

    // Suaviza o cutoff (20 ms) para não estalar ao girar o knob, automatizar ou modular.
    // Multiplicativo: anda em proporção (oitavas), como o ouvido percebe frequência.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoffSmoothed;

    // Parte da matriz no cutoff e no volume dos osciladores, interpolada só entre um recálculo
    // e o próximo (32 amostras). Separada da suavização de 20 ms dos knobs para que um LFO
    // rápido não seja "abafado" (RF11).
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> cutoffModRatio; // multiplica o cutoff
    std::array<juce::SmoothedValue<float>, ParamIDs::numOscillators> oscLevelMods;        // soma ao volume

    // LFOs desta voz. No modo Free, a fase é copiada da posição global a cada bloco.
    std::array<Lfo, ParamIDs::numLfos> lfos;
    std::array<std::uint64_t, ParamIDs::numLfos> noteSeeds {}; // Sample & Hold no modo Retrigger
    juce::Random random; // sorteia as sementes a cada nota (não aloca)

    // Matriz de modulação: fontes suavizadas (MIDI chega em degraus de 1/127) e
    // deslocamento atual de cada destino.
    juce::SmoothedValue<float> modWheelSmoothed, aftertouchSmoothed;
    float polyAftertouch = 0.0f;
    float modEnvValue    = 0.0f;
    ModMatrix::DestOffsets modOffsets {};
    static constexpr int modUpdateInterval = 32; // amostras entre recálculos da matriz
    int samplesUntilModUpdate = 0;

    float level = 0.0f; // volume da nota (vem da velocity)

    // Contagem para o gráfico: amostras desde o note-on (ou desde o note-off, se released).
    int   samplesInStage = 0;
    bool  released = false;
    float lastEnvelopeLevel = 0.0f;

    bool monoHandoffPending = false;
    bool monoRetrigger      = true;
};
