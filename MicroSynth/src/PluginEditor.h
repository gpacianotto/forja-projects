#pragma once

#include <array>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"
#include "Parameters.h"

class PluginProcessor;

// Conteúdo de uma aba: grupos de parâmetros dispostos em linhas, com uma nota opcional embaixo.
// Os grupos não pertencem à página (são membros do editor); ela só os posiciona.
class TabPage final : public juce::Component
{
public:
    void addRow (std::initializer_list<ParameterGroup*> groups);

    // Altura máxima de cada linha, para os knobs não ficarem gigantes numa aba com poucos controles.
    void setMaxRowHeight (int height) { maxRowHeight = height; }
    void setNote (const juce::String& text);

    void resized() override;

private:
    std::vector<std::vector<ParameterGroup*>> rows;
    int maxRowHeight = 10000;
    juce::Label note;
};

// Interface própria do MicroSynth, organizada em abas (RF8):
//   Oscillators  — os 3 osciladores (RF6)
//   Amp & Filter — modo de vozes (RF7), volume, envelope e filtro
//   Tuning       — afinação, com trava contra mudanças acidentais
class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void setTuningLocked (bool locked);

    // Precisa existir enquanto os componentes usarem; por isso é o primeiro membro
    // (construído primeiro, destruído por último).
    juce::LookAndFeel_V4 lookAndFeel { juce::LookAndFeel_V4::getMidnightColourScheme() };

    std::array<ParameterGroup, ParamIDs::numOscillators> oscillatorGroups { ParameterGroup { "Osc 1" },
                                                                            ParameterGroup { "Osc 2" },
                                                                            ParameterGroup { "Osc 3" } };
    ParameterGroup voiceGroup    { "Voice" };
    ParameterGroup outputGroup   { "Output" };
    ParameterGroup envelopeGroup { "Envelope" };
    ParameterGroup filterGroup   { "Filter" };
    ParameterGroup tuningGroup   { "Tuning" };

    TabPage oscillatorsPage, ampFilterPage, tuningPage;

    // Declarado depois das páginas: é destruído antes delas.
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    // Knobs que a trava de afinação desabilita (não são donos: o tuningGroup é).
    std::vector<juce::Component*> tuningControls;

    // Liga o parâmetro tuningLock à interface; o callback roda sempre na thread da interface.
    std::unique_ptr<juce::ParameterAttachment> tuningLockAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
