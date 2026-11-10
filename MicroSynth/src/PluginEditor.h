#pragma once

#include <array>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"
#include "ModulationUI.h"
#include "Parameters.h"

class PluginProcessor;

// Conteúdo de uma aba: grupos de parâmetros dispostos em linhas, com uma nota opcional embaixo.
// Os grupos não pertencem à página (são membros do editor); ela só os posiciona.
class TabPage final : public juce::Component
{
public:
    // weight: altura relativa da linha (2 = o dobro de uma linha com peso 1).
    void addRow (std::initializer_list<ParameterGroup*> groups, int weight = 1);

    // Altura máxima de cada linha, para os knobs não ficarem gigantes numa aba com poucos controles.
    void setMaxRowHeight (int height) { maxRowHeight = height; }
    void setNote (const juce::String& text);

    void resized() override;

private:
    struct Row
    {
        std::vector<ParameterGroup*> groups;
        int weight = 1;
    };
    std::vector<Row> rows;
    int maxRowHeight = 10000;
    juce::Label note;
};

// Interface própria do MicroSynth, organizada em abas (RF8):
//   Oscillators  — os 3 osciladores (RF6)
//   Amp & Filter — modo de vozes (RF7), volume, envelope e filtro
//   Matrix       — envelope de modulação e os 8 slots da matriz (RF10)
//   Tuning       — afinação, com trava contra mudanças acidentais
//
// As fontes de modulação ficam no cabeçalho, visíveis em todas as abas, para poderem ser
// arrastadas até qualquer knob. O editor é o DragAndDropContainer que coordena o arrasto.
class PluginEditor final : public juce::AudioProcessorEditor, public juce::DragAndDropContainer
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

    // Usado pelos knobs (que guardam um ponteiro para ele): declarado antes dos grupos,
    // é destruído depois deles.
    ModulationModel modulationModel;

    std::array<ParameterGroup, ParamIDs::numOscillators> oscillatorGroups { ParameterGroup { "Osc 1" },
                                                                            ParameterGroup { "Osc 2" },
                                                                            ParameterGroup { "Osc 3" } };
    ParameterGroup voiceGroup    { "Voice" };
    ParameterGroup outputGroup   { "Output" };
    ParameterGroup envelopeGroup { "Envelope" };
    ParameterGroup filterGroup   { "Filter" };
    ParameterGroup tuningGroup   { "Tuning" };
    ParameterGroup modEnvGroup   { "Mod Env" };
    ParameterGroup slotsGroup    { "Modulation Matrix" };

    TabPage oscillatorsPage, ampFilterPage, matrixPage, tuningPage;

    // Alças das fontes de modulação, no cabeçalho.
    std::vector<std::unique_ptr<SourceChip>> sourceChips;

    // Mostra as dicas (tooltips) das alças e dos botões.
    juce::TooltipWindow tooltipWindow { this, 600 };

    // Declarado depois das páginas: é destruído antes delas.
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    // Knobs que a trava de afinação desabilita (não são donos: o tuningGroup é).
    std::vector<juce::Component*> tuningControls;

    // Liga o parâmetro tuningLock à interface; o callback roda sempre na thread da interface.
    std::unique_ptr<juce::ParameterAttachment> tuningLockAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
