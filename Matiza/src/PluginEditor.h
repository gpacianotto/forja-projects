#pragma once

#include <array>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "AboutOverlay.h"
#include "Controls.h"
#include "ModulationUI.h"
#include "Palette.h"
#include "Parameters.h"
#include "HarmonicsView.h"
#include "ScaleLines.h"
#include "ShapeEditor.h"

class PluginProcessor;

// Conteúdo de uma aba: grupos de parâmetros dispostos em linhas, com uma nota opcional embaixo.
// Os grupos não pertencem à página (são membros do editor); ela só os posiciona.
class TabPage final : public juce::Component
{
public:
    // weight: altura relativa da linha (2 = o dobro de uma linha com peso 1).
    // rowMaxHeight > 0: altura máxima só desta linha; o que sobrar vai para as linhas sem limite
    // próprio (ex.: na aba Tuning, os knobs ficam baixos e o gráfico dos steps cresce).
    void addRow (std::initializer_list<ParameterGroup*> groups, int weight = 1, int rowMaxHeight = 0);

    // Linha ocupada por um componente qualquer (ex.: sub-abas), na largura toda. Como os
    // grupos, ele não pertence à página.
    void addComponentRow (juce::Component& component, int weight = 1);

    // Altura máxima de cada linha, para os knobs não ficarem gigantes numa aba com poucos controles.
    void setMaxRowHeight (int height) { maxRowHeight = height; }
    void setNote (const juce::String& text);

    // Altura fixa reservada para a nota no rodapé. 0 (padrão) = a nota fica com o que sobrar
    // embaixo das linhas.
    void setNoteHeight (int height) { noteHeight = height; }

    void resized() override;

private:
    struct Row
    {
        std::vector<ParameterGroup*> groups;
        juce::Component* component = nullptr; // linha de componente (addComponentRow)
        int weight = 1;
        int maxHeight = 0; // 0 = sem limite próprio
    };
    std::vector<Row> rows;
    int maxRowHeight = 10000;
    int noteHeight = 0;
    juce::Label note;
};

// Interface própria do Matiza, organizada em abas (RF8):
//   Oscillators  — os 3 osciladores (RF6), um por sub-aba, com o unison embaixo (RF12, RF19)
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

    // Linhas da escala atual em volta do step 0, para o gráfico do unison (RF19).
    ScaleLines::Lines currentScaleLines();

    // Retrato do gráfico dos harmônicos (RF21): forma do primeiro oscilador ligado, nota mais
    // grave segurada (ou a nota de referência) e a afinação atual, com a mesma conta da voz.
    HarmonicsView::Snapshot harmonicsSnapshot();

    // Receita das formas desenhadas, refeita só quando os pontos mudam (a DFT custa ~1 ms).
    std::array<ShapePoints, ParamIDs::numOscillators> recipePoints {};
    std::array<AlignedSpectrum::Recipe, ParamIDs::numOscillators> customRecipes {};
    std::array<bool, ParamIDs::numOscillators> customRecipeReady {};

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
    std::array<ParameterGroup, ParamIDs::numOscillators> unisonGroups { ParameterGroup { "Unison" },
                                                                        ParameterGroup { "Unison" },
                                                                        ParameterGroup { "Unison" } };
    ParameterGroup voiceGroup    { "Voice" };
    ParameterGroup outputGroup   { "Output" };
    ParameterGroup envelopeGroup { "Envelope" };
    ParameterGroup filterGroup   { "Filter" };
    ParameterGroup tuningGroup   { "Tuning" };
    ParameterGroup stepsGroup    { "Steps" }; // gráfico dos steps da EDO (RF16)
    ParameterGroup harmonicsGroup { "Harmonic Align" }; // gráfico dos harmônicos + knob Align (RF21)
    ParameterGroup slotsGroup    { "Modulation Matrix" };
    std::array<ParameterGroup, ParamIDs::numLfos> lfoGroups { ParameterGroup { "LFO 1" },
                                                              ParameterGroup { "LFO 2" },
                                                              ParameterGroup { "LFO 3" } };

    TabPage ampFilterPage, matrixPage, lfoPage, tuningPage;

    // Aba Oscillators: uma sub-aba por oscilador (oscilador em cima, unison embaixo).
    // As sub-abas são declaradas depois das páginas: são destruídas antes delas.
    std::array<TabPage, ParamIDs::numOscillators> oscillatorPages;
    juce::TabbedComponent oscillatorTabs { juce::TabbedButtonBar::TabsAtTop };

    // Aba Tuning: knobs em cima e, embaixo, sub-abas Steps (RF16/RF17) e Harmonic Align (RF21).
    TabPage stepsSubPage, harmonicsSubPage;
    juce::TabbedComponent tuningSubTabs { juce::TabbedButtonBar::TabsAtTop };

    // Alças das fontes de modulação, no cabeçalho.
    std::vector<std::unique_ptr<SourceChip>> sourceChips;

    // Mostra as dicas (tooltips) das alças e dos botões.
    juce::TooltipWindow tooltipWindow { this, 600 };

    // Declarado depois das páginas: é destruído antes delas.
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };

    // Knobs da afinação (não são donos: o tuningGroup é). A trava (Lock) desabilita todos; o
    // master do MTS-ESP (RF18) desabilita só os da afinação interna (EDO, Base Freq, Ref Note),
    // porque o Bend Range continua valendo com o master.
    std::vector<juce::Component*> internalTuningControls;
    juce::Component* bendRangeControl = nullptr;
    bool tuningLocked = false;
    bool followingMaster = false;
    void updateTuningControls();

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

    // Botão No Fund de cada oscilador (RF22): apagado com Sine, em que não vale. O attachment
    // avisa quando a forma de onda muda (inclusive por automação); declarado depois dos grupos,
    // é destruído antes do botão.
    std::array<juce::Component*, ParamIDs::numOscillators> noFundControls {};
    std::array<std::unique_ptr<juce::ParameterAttachment>, ParamIDs::numOscillators> noFundWaveAttachments;

    // Logo MTZ no cabeçalho; clicar abre a tela Sobre.
    juce::DrawableButton logoButton { "About Matiza", juce::DrawableButton::ImageFitted };

    // Por último: fica por cima de todos os outros componentes quando aberta.
    AboutOverlay aboutOverlay;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
