#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>

#include "HarmonicsView.h"
#include "PluginProcessor.h"
#include "ScaleLines.h"
#include "ShapePoints.h"

// Gráficos da aba Amp & Filter (RF9).
//
// Cada gráfico lê os parâmetros direto dos atômicos do APVTS num Timer (30 vezes por
// segundo, na thread da interface) e só se redesenha quando algo mudou. Assim acompanha
// knobs, automação do Ableton e o ponto do envelope, sem nunca tocar na thread de áudio.

// Curva de resposta do filtro passa-baixa: frequência (log, 20 Hz–20 kHz) × ganho (dB).
// RF20: com Cutoff ou Resonance modulados e uma nota tocando, desenha a curva com os valores
// modulados (da última voz tocada) e deixa a curva dos knobs apagada por trás.
class FilterGraph final : public juce::Component, private juce::Timer
{
public:
    FilterGraph (juce::AudioProcessorValueTreeState& apvts, const ModulationDisplay& modulation);
    ~FilterGraph() override { stopTimer(); }

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    // Valores modulados agora (iguais aos dos knobs sem modulação ou sem nota).
    struct Values { float cutoff = 0.0f; float reso = 0.0f; bool operator== (const Values&) const = default; };
    Values modulatedValues() const;

    std::atomic<float>* cutoffParam = nullptr;
    std::atomic<float>* resoParam   = nullptr;
    juce::NormalisableRange<float> cutoffRange, resoRange; // as mesmas faixas que a voz usa
    const ModulationDisplay& modulation;

    float drawnCutoff = -1.0f, drawnReso = -1.0f; // valores dos knobs no último desenho
    Values drawnModulated { -1.0f, -1.0f };       // valores modulados no último desenho
};

// Um ciclo da forma de onda de um oscilador (card "gráfico das ondas dos osciladores").
// A altura acompanha o Level; oscilador desligado aparece apagado. Com a forma Custom, desenha
// os pontos (RF15). Clicar abre o editor de pontos (openEditor, ligado pelo editor do plugin).
class OscillatorGraph final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    OscillatorGraph (juce::AudioProcessorValueTreeState& apvts, std::size_t oscIndex,
                     std::function<ShapePoints()> customPoints,
                     std::function<void (juce::Component&)> openEditor);
    ~OscillatorGraph() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    std::atomic<float>* onParam    = nullptr;
    std::atomic<float>* waveParam  = nullptr;
    std::atomic<float>* levelParam = nullptr;
    float drawnOn = -1.0f, drawnWave = -1.0f, drawnLevel = -1.0f;

    std::function<ShapePoints()> customPoints;
    std::function<void (juce::Component&)> openEditor;
    ShapePoints drawnPoints;
};

// Gráfico do unison de um oscilador (RF19), ao lado dos knobs Unison, Detune, Blend e Width:
// uma barra por cópia num eixo de −100 a +100 cents (posição = desvio, altura = volume), a
// régua L—C—R do Width com um tique por cópia e, atrás, as linhas da escala atual (steps
// vizinhos da EDO, da afinação desenhada ou do master do MTS-ESP). Sempre visível; usa a mesma
// conta do áudio (Unison.h).
class UnisonGraph final : public juce::Component, private juce::Timer
{
public:
    UnisonGraph (juce::AudioProcessorValueTreeState& apvts, std::size_t oscIndex,
                 std::function<ScaleLines::Lines()> scaleLines);
    ~UnisonGraph() override { stopTimer(); }

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    std::array<float, 5> readValues() const; // on, unison, detune, blend, width

    std::atomic<float>* onParam     = nullptr;
    std::atomic<float>* unisonParam = nullptr;
    std::atomic<float>* detuneParam = nullptr;
    std::atomic<float>* blendParam  = nullptr;
    std::atomic<float>* widthParam  = nullptr;
    std::function<ScaleLines::Lines()> scaleLines;

    std::array<float, 5> drawnValues { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
    ScaleLines::Lines drawnLines;
};

// Gráfico dos harmônicos alinhados (RF21): os 24 primeiros harmônicos da forma de onda sobre as
// alturas da escala acima da nota. Eixo em cents (escala do ouvido): 1×, 2×, 4×… ficam
// equidistantes. Cada barra mostra onde o harmônico toca com o Align atual (altura = volume, em
// dB); um tracinho fraco marca a posição natural. Girar o Align leva as barras até as linhas.
// O editor monta o retrato (HarmonicsView::Snapshot) num timer de 30 Hz; só redesenha se mudou.
class HarmonicsGraph final : public juce::Component, private juce::Timer
{
public:
    explicit HarmonicsGraph (std::function<HarmonicsView::Snapshot()> source);
    ~HarmonicsGraph() override { stopTimer(); }

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    std::function<HarmonicsView::Snapshot()> source;
    HarmonicsView::Snapshot drawn;
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
