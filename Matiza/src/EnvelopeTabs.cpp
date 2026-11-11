#include "EnvelopeTabs.h"
#include "Palette.h"

// ----------------------------------------------------------------------------- EnvelopePanel

EnvelopePanel::EnvelopePanel (juce::AudioProcessorValueTreeState& apvts, EnvelopeGraph::ParamIDs4 ids,
                              const PluginProcessor::EnvelopeDisplay& display)
    : graph   (apvts, ids, display),
      attack  (apvts, ids.attack,  "Attack"),
      decay   (apvts, ids.decay,   "Decay"),
      sustain (apvts, ids.sustain, "Sustain"),
      release (apvts, ids.release, "Release")
{
    for (auto* child : std::initializer_list<juce::Component*> { &graph, &attack, &decay, &sustain, &release })
        addAndMakeVisible (*child);
}

void EnvelopePanel::resized()
{
    auto area = getLocalBounds().reduced (0, 4);

    // Metade de cima para o gráfico, como no grupo Filter ao lado.
    graph.setBounds (area.removeFromTop (area.getHeight() / 2).reduced (2, 0));
    area.removeFromTop (4);

    juce::FlexBox row;
    row.flexDirection = juce::FlexBox::Direction::row;
    for (auto* knob : { &attack, &decay, &sustain, &release })
        row.items.add (juce::FlexItem (*knob).withFlex (1.0f).withMargin (2.0f));

    row.performLayout (area);
}

// ----------------------------------------------------------------------------- EnvelopeTabs

EnvelopeTabs::EnvelopeTabs (juce::AudioProcessorValueTreeState& apvts,
                            const PluginProcessor::EnvelopeDisplay& ampDisplay,
                            const PluginProcessor::EnvelopeDisplay& modDisplay)
    : ampPanel (apvts, { ParamIDs::attack, ParamIDs::decay, ParamIDs::sustain, ParamIDs::release }, ampDisplay),
      modPanel (apvts, { ParamIDs::modEnvAttack, ParamIDs::modEnvDecay, ParamIDs::modEnvSustain, ParamIDs::modEnvRelease },
                modDisplay)
{
    tabs.setTabBarDepth (24);
    tabs.setOutline (0);
    tabs.setIndent (0);
    // false = as abas não apagam os painéis (eles são membros desta classe).
    tabs.addTab ("Amp Env", juce::Colours::transparentBlack, &ampPanel, false);
    tabs.addTab ("Mod Env", juce::Colours::transparentBlack, &modPanel, false);
    addAndMakeVisible (tabs);
    updateColours();
}

void EnvelopeTabs::updateColours()
{
    // Mesma cor de fundo do ParameterGroup, para as abas parecerem parte do grupo.
    const auto background = Palette::panel;

    for (int i = 0; i < tabs.getNumTabs(); ++i)
        tabs.setTabBackgroundColour (i, background);
}

void EnvelopeTabs::resized()
{
    tabs.setBounds (getLocalBounds());
}
