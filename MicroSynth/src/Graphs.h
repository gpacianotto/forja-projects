#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

// Gráficos da aba Amp & Filter (RF9).
//
// Cada gráfico lê os parâmetros direto dos atômicos do APVTS num Timer (30 vezes por
// segundo, na thread da interface) e só se redesenha quando algo mudou. Assim acompanha
// knobs, automação do Ableton e o ponto do envelope, sem nunca tocar na thread de áudio.

// Curva de resposta do filtro passa-baixa: frequência (log, 20 Hz–20 kHz) × ganho (dB).
class FilterGraph final : public juce::Component, private juce::Timer
{
public:
    explicit FilterGraph (juce::AudioProcessorValueTreeState& apvts);
    ~FilterGraph() override { stopTimer(); }

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    std::atomic<float>* cutoffParam = nullptr;
    std::atomic<float>* resoParam   = nullptr;
    float drawnCutoff = -1.0f, drawnReso = -1.0f; // valores do último desenho
};

// Um ciclo da forma de onda de um oscilador (card "gráfico das ondas dos osciladores").
// A altura acompanha o Level; oscilador desligado aparece apagado.
class OscillatorGraph final : public juce::Component, private juce::Timer
{
public:
    OscillatorGraph (juce::AudioProcessorValueTreeState& apvts, std::size_t oscIndex);
    ~OscillatorGraph() override { stopTimer(); }

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    std::atomic<float>* onParam    = nullptr;
    std::atomic<float>* waveParam  = nullptr;
    std::atomic<float>* levelParam = nullptr;
    float drawnOn = -1.0f, drawnWave = -1.0f, drawnLevel = -1.0f;
};

// Desenho do ADSR, com um ponto na posição atual do envelope da última nota tocada.
// Serve para o envelope de volume e para o de modulação: recebe os IDs dos 4 parâmetros.
class EnvelopeGraph final : public juce::Component, private juce::Timer
{
public:
    struct ParamIDs4 { const char* attack; const char* decay; const char* sustain; const char* release; };

    EnvelopeGraph (juce::AudioProcessorValueTreeState& apvts, ParamIDs4 ids,
                   const PluginProcessor::EnvelopeDisplay& display);
    ~EnvelopeGraph() override { stopTimer(); }

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    std::atomic<float>* attackParam  = nullptr;
    std::atomic<float>* decayParam   = nullptr;
    std::atomic<float>* sustainParam = nullptr;
    std::atomic<float>* releaseParam = nullptr;
    const PluginProcessor::EnvelopeDisplay& envelopeDisplay;

    // Valores do último desenho (ms, 0..1), para saber quando redesenhar.
    float drawnAttack = -1.0f, drawnDecay = -1.0f, drawnSustain = -1.0f, drawnRelease = -1.0f;
    bool  dotWasVisible = false;
};
