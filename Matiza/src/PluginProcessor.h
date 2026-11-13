#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <memory>

#include "CustomShapes.h"
#include "MatizaSynthesiser.h"
#include "Parameters.h"
#include "TripleBuffer.h"
#include "TuningEngine.h"
#include "TuningTable.h"

// Processador do Matiza: recebe MIDI do host e devolve áudio estéreo.
class PluginProcessor final : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }   // instrumento: recebe notas
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    // Posição do envelope da última nota tocada, para o ponto do gráfico (RF9).
    // A thread de áudio escreve e a interface lê; atômicos = sem lock.
    struct EnvelopeDisplay
    {
        std::atomic<int>   stage { 0 };            // GraphMath::EnvelopeStage
        std::atomic<float> secondsInStage { 0.0f };
        std::atomic<float> level { 0.0f };
    };
    EnvelopeDisplay envelopeDisplay;
    EnvelopeDisplay modEnvDisplay; // envelope de modulação (RF10)

    // --- Formas desenhadas (RF15). Só na thread da interface. ---
    const ShapePoints& getLfoPoints (std::size_t lfo) const { return customShapes->getLfoPoints (lfo); }
    const ShapePoints& getOscPoints (std::size_t osc) const { return customShapes->getOscPoints (osc); }
    void setLfoPoints (std::size_t lfo, const ShapePoints& points);
    void setOscPoints (std::size_t osc, const ShapePoints& points);

    // Fase atual de cada LFO para o ponto do editor (0..1), ou -1 sem nota tocando no modo
    // Retrigger. A thread de áudio escreve, a interface lê.
    std::array<std::atomic<float>, ParamIDs::numLfos> lfoDisplayPhase {};

    // --- Afinação desenhada (RF17). Só na thread da interface. ---
    // A tabela vale só quando table.edo == edo atual; TuningTable {} = nenhuma (afinação igual).
    const TuningTable& getTuningTable() const { return tuningTableUi; }
    void setTuningTable (const TuningTable& table);

    static constexpr int numVoices = 8;

    // Altura de cada voz para o gráfico dos steps (RF16). A thread de áudio escreve uma vez por
    // bloco, a interface lê. Os três campos são atômicos separados: ler um campo "novo" e outro
    // "velho" no mesmo quadro só desenharia um quadro errado, sem nenhum risco.
    struct VoicePitchDisplay
    {
        std::atomic<bool>  held { false };
        std::atomic<int>   midiNote { 0 };
        std::atomic<float> centsFromBase { 0.0f };
    };
    std::array<VoicePitchDisplay, numVoices> voicePitchDisplay {};

private:
    // Nomes das propriedades do estado onde os pontos são salvos (junto com os parâmetros).
    static juce::Identifier lfoPointsId (std::size_t lfo) { return "lfo" + juce::String (lfo + 1) + "Points"; }
    static juce::Identifier oscPointsId (std::size_t osc) { return "osc" + juce::String (osc + 1) + "Points"; }
    void loadShapesFromState();

    // Afinação desenhada (RF17): cópia da interface + entrega ao áudio sem lock.
    static inline const juce::Identifier tuningTableId { "tuningTable" };
    void loadTuningTableFromState();
    TuningTable tuningTableUi;
    TripleBuffer<TuningTable> tuningTables;

    // No heap: as tabelas ocupam ~1,6 MB (ver CustomShapes.h).
    std::unique_ptr<CustomShapes> customShapes = std::make_unique<CustomShapes>();

    // Parâmetros lidos na thread de áudio (atômicos, donos são o apvts)
    std::atomic<float>* edoParam       = nullptr;
    std::atomic<float>* baseHzParam    = nullptr;
    std::atomic<float>* refNoteParam   = nullptr;
    std::atomic<float>* bendStepsParam = nullptr;
    std::atomic<float>* attackParam    = nullptr;
    std::atomic<float>* decayParam     = nullptr;
    std::atomic<float>* sustainParam   = nullptr;
    std::atomic<float>* releaseParam   = nullptr;
    std::atomic<float>* cutoffParam    = nullptr;
    std::atomic<float>* resoParam      = nullptr;
    std::atomic<float>* gainParam      = nullptr;
    std::atomic<float>* voiceModeParam = nullptr;
    std::atomic<float>* legatoParam    = nullptr;
    std::atomic<float>* glideParam     = nullptr;

    // Um conjunto de ponteiros por oscilador (RF6).
    struct OscillatorParamPointers
    {
        std::atomic<float>* on     = nullptr;
        std::atomic<float>* wave   = nullptr;
        std::atomic<float>* level  = nullptr;
        std::atomic<float>* octave = nullptr;
        std::atomic<float>* steps  = nullptr;
        std::atomic<float>* fine   = nullptr;
        std::atomic<float>* unison = nullptr; // RF12
        std::atomic<float>* detune = nullptr;
        std::atomic<float>* blend  = nullptr;
        std::atomic<float>* width  = nullptr;
    };
    std::array<OscillatorParamPointers, ParamIDs::numOscillators> oscParams;

    // Matriz de modulação (RF10)
    struct ModSlotParamPointers
    {
        std::atomic<float>* source = nullptr;
        std::atomic<float>* dest   = nullptr;
        std::atomic<float>* amount = nullptr;
    };
    std::array<ModSlotParamPointers, ParamIDs::numModSlots> modSlotParams;
    // LFOs (RF11)
    struct LfoParamPointers
    {
        std::atomic<float>* shape    = nullptr;
        std::atomic<float>* rate     = nullptr;
        std::atomic<float>* sync     = nullptr;
        std::atomic<float>* division = nullptr;
        std::atomic<float>* trigger  = nullptr;
    };
    std::array<LfoParamPointers, ParamIDs::numLfos> lfoParams;

    // Relógio dos LFOs no modo Free (um por LFO) e a frequência efetiva deste bloco.
    std::array<Lfo, ParamIDs::numLfos> freeLfos;
    std::array<double, ParamIDs::numLfos> lfoRatesHz {};

    std::atomic<float>* modEnvAttackParam  = nullptr;
    std::atomic<float>* modEnvDecayParam   = nullptr;
    std::atomic<float>* modEnvSustainParam = nullptr;
    std::atomic<float>* modEnvReleaseParam = nullptr;

    // Atualizado no início de cada processBlock; as vozes consultam no note-on.
    // Declarado antes do synth: membros são construídos na ordem de declaração e as vozes
    // guardam uma referência para ele.
    TuningEngine tuning;
    GlobalModState globalMod; // mod wheel e aftertouch de canal (RF10)
    ModRanges modRanges;      // faixas do cutoff e da ressonância, para a matriz

    MatizaSynthesiser synth { tuning, globalMod }; // distribui as notas MIDI entre as vozes (RF4, RF7)

    juce::SmoothedValue<float> gainSmoothed; // volume de saída, com rampa de 20 ms

    void updateTuning();
    void updateVoices();
    void publishEnvelopeDisplay();
    void publishPitchDisplay();
    void updateLfoClock();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
