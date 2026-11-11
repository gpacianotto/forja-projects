#pragma once

#include <array>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"
#include "ModulationUI.h"
#include "Palette.h"
#include "Parameters.h"
#include "ShapeEditor.h"

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

// Interface própria do Matiza, organizada em abas (RF8):
//   Oscillators  — os 3 osciladores (RF6)
//   Amp & Filter — modo de vozes (RF7), volume, envelopes (Amp Env / Mod Env) e filtro
//   Matrix       — os 8 slots da matriz de modulação (RF10)
//   LFOs         — os 3 LFOs (RF11)
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

    // Editores de forma desenhada (RF15), ligados aos pontos do processador e ao parâmetro
    // de forma (a primeira edição muda a forma para Custom).
    std::unique_ptr<ShapeEditor> makeLfoShapeEditor (std::size_t lfo);
    std::unique_ptr<ShapeEditor> makeOscShapeEditor (std::size_t osc);
    void openOscShapeEditor (std::size_t osc, juce::Component& target);

    PluginProcessor& pluginProcessor;
    void setLfoSynced (size_t lfoIndex, bool synced);

    // Precisa existir enquanto os componentes usarem; por isso é o primeiro membro
    // (construído primeiro, destruído por último).
    juce::LookAndFeel_V4 lookAndFeel { Palette::colourScheme() }; // paleta Têmpera (Palette.h)

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
    ParameterGroup slotsGroup    { "Modulation Matrix" };
    std::array<ParameterGroup, ParamIDs::numLfos> lfoGroups { ParameterGroup { "LFO 1" },
                                                              ParameterGroup { "LFO 2" },
                                                              ParameterGroup { "LFO 3" } };

    TabPage oscillatorsPage, ampFilterPage, matrixPage, lfoPage, tuningPage;

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

    // Com Sync ligado, o knob Rate fica desabilitado e a divisão habilitada (e vice-versa).
    struct LfoSyncControls
    {
        juce::Component* rate = nullptr;
        juce::Component* division = nullptr;
        std::unique_ptr<juce::ParameterAttachment> syncAttachment;
    };
    std::array<LfoSyncControls, ParamIDs::numLfos> lfoSyncControls;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
