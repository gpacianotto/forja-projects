#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

#include "MicroSynthesiser.h"
#include "Parameters.h"
#include "TuningEngine.h"

// Processador do MicroSynth: recebe MIDI do host e devolve áudio estéreo.
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

private:
    static constexpr int numVoices = 8;

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
    };
    std::array<OscillatorParamPointers, ParamIDs::numOscillators> oscParams;

    // Atualizado no início de cada processBlock; as vozes consultam no note-on.
    // Declarado antes do synth: membros são construídos na ordem de declaração e as vozes
    // guardam uma referência para ele.
    TuningEngine tuning;

    MicroSynthesiser synth { tuning }; // distribui as notas MIDI entre as vozes (com a guarda do RF4)

    juce::SmoothedValue<float> gainSmoothed; // volume de saída, com rampa de 20 ms

    void updateTuning();
    void updateVoices();
    void publishEnvelopeDisplay();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
