#pragma once

#include <array>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

// Gráfico dos steps da EDO na aba Tuning (RF16).
//
// Mostra onde cada passo da EDO cai dentro da oitava, em cents (a escala que o ouvido percebe),
// em duas visões que o usuário escolhe nos botões do canto:
//   Circle — uma volta = uma oitava; uma linha do centro até a borda por step, step 0 no topo.
//   Line   — a mesma oitava da esquerda (0 ¢) para a direita (1200 ¢), com linhas verticais.
//
// Notas seguradas acendem a linha do seu step (brilho que se apaga ~300 ms depois de soltar),
// e um ponto mostra a altura que está soando, andando entre as linhas com pitch bend e glide.
//
// Como os outros gráficos (Graphs.h), lê tudo de atômicos num Timer de 30 Hz, na thread da
// interface, e só redesenha quando algo mudou. Nada aqui muda o som.
class TuningStepsView final : public juce::Component, private juce::Timer
{
public:
    explicit TuningStepsView (PluginProcessor& processor);
    ~TuningStepsView() override { stopTimer(); }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum class View { circle, line };

    // Nota segurada, para a lista ao lado do gráfico.
    struct HeldNote
    {
        int midiNote = 0;
        int step = 0;
        int octave = 0;

        bool operator== (const HeldNote&) const = default; // compara campo a campo (C++20)
    };

    void timerCallback() override;

    // Escolhas da interface (sub-aba e botão 12-TET), salvas no estado do plugin (não são
    // parâmetros do host). save = false ao aplicar um valor que veio do estado.
    void setView (View newView, bool save);
    void setTwelveTet (bool on, bool save);
    void syncUiStateFromTree();

    void paintHeader (juce::Graphics&, juce::Rectangle<float> area);
    void paintCircle (juce::Graphics&, juce::Rectangle<float> area);
    void paintLine (juce::Graphics&, juce::Rectangle<float> area);
    void paintHeldList (juce::Graphics&, juce::Rectangle<float> area, bool singleRow);

    // Uma linha de step: cor, espessura e brilho conforme o step e se está aceso.
    void drawStepLine (juce::Graphics&, juce::Line<float> line, int step) const;
    juce::Colour labelColour (int step) const;

    PluginProcessor& pluginProcessor;
    juce::AudioProcessorValueTreeState& apvts;
    std::atomic<float>* edoParam     = nullptr;
    std::atomic<float>* baseHzParam  = nullptr;
    std::atomic<float>* refNoteParam = nullptr;

    juce::TextButton circleButton { "Circle" }, lineButton { "Line" };
    juce::ToggleButton twelveTetButton { "12-TET" };

    View view = View::circle;
    bool showTwelveTet = false;

    // Estado do último desenho.
    static constexpr int maxEdo = 72; // máximo do parâmetro edo
    int    edo = 12;
    double baseHz = 440.0;
    int    refNote = 69;
    std::array<float, maxEdo> glow {}; // brilho de cada step, 0..1

    // Ponto de cada voz segurada: posição em cents dentro da oitava, ou -1 sem ponto.
    std::array<float, PluginProcessor::numVoices> dotCents {};
    std::vector<HeldNote> heldNotes; // ordenadas pela nota, sem repetição

    juce::Rectangle<int> headerArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TuningStepsView)
};
