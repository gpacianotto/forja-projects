#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"
#include "Graphs.h"

// Um envelope completo: gráfico em cima e os 4 knobs (Attack, Decay, Sustain, Release) embaixo.
class EnvelopePanel final : public juce::Component
{
public:
    EnvelopePanel (juce::AudioProcessorValueTreeState& apvts, EnvelopeGraph::ParamIDs4 ids,
                   const PluginProcessor::EnvelopeDisplay& display);

    void resized() override;

private:
    EnvelopeGraph graph;
    Knob attack, decay, sustain, release;
};

// Grupo Envelope com abas internas (card "Mod Env em aba dentro do grupo Envelope"):
// "Amp Env" é o envelope de volume e "Mod Env" o envelope de modulação (RF10).
// Só muda onde os controles aparecem: os parâmetros e seus IDs continuam os mesmos.
class EnvelopeTabs final : public juce::Component
{
public:
    EnvelopeTabs (juce::AudioProcessorValueTreeState& apvts,
                  const PluginProcessor::EnvelopeDisplay& ampDisplay,
                  const PluginProcessor::EnvelopeDisplay& modDisplay);

    void resized() override;

    // O tema (LookAndFeel) só é conhecido depois que o componente entra na janela; estes dois
    // avisos chegam nesse momento e quando o tema muda, e recolorem as abas.
    void parentHierarchyChanged() override { updateColours(); }
    void lookAndFeelChanged() override     { updateColours(); }

private:
    void updateColours();

    EnvelopePanel ampPanel, modPanel;

    // Declarado depois dos painéis: é destruído antes deles (as abas só apontam para eles).
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
};
