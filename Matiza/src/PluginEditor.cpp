#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "EnvelopeTabs.h"
#include "Graphs.h"
#include "TuningStepsView.h"

namespace
{
    constexpr int headerHeight = 36;
    constexpr int margin       = 12;
    constexpr int gap          = 10;
    constexpr int tabBarDepth  = 30;
    constexpr int titleWidth   = 140; // espaço do nome "Matiza" no cabeçalho
    constexpr int versionWidth = 60;

    // Tamanho inicial e limites da janela redimensionável. O mínimo é o próprio tamanho
    // inicial: menor que isso, os knobs e os gráficos ficam apertados.
    constexpr int defaultWidth  = 840, defaultHeight = 560;
    constexpr int minWidth      = defaultWidth, minHeight = defaultHeight;
    constexpr int maxWidth      = 1680, maxHeight    = 1120;

    // Uma linha de grupos; cada grupo ganha largura proporcional ao seu número de controles,
    // para que todos os knobs da linha tenham o mesmo tamanho.
    void layoutRow (juce::Rectangle<int> area, const std::vector<ParameterGroup*>& groups)
    {
        juce::FlexBox row;
        row.flexDirection = juce::FlexBox::Direction::row;

        for (auto* group : groups)
            row.items.add (juce::FlexItem (*group)
                               .withFlex (static_cast<float> (group->getLayoutWeight()))
                               .withMargin (juce::FlexItem::Margin (0.0f, gap / 2.0f, 0.0f, gap / 2.0f)));

        row.performLayout (area.expanded (gap / 2, 0));
    }

    // Muda um parâmetro de escolha para o índice dado, avisando o host (gesto completo).
    void setChoiceIndex (juce::AudioProcessorValueTreeState& apvts, const char* paramID, int index)
    {
        auto* param = apvts.getParameter (paramID);
        if (param == nullptr || juce::roundToInt (param->convertFrom0to1 (param->getValue())) == index)
            return;

        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (index)));
        param->endChangeGesture();
    }

    int choiceIndex (juce::AudioProcessorValueTreeState& apvts, const char* paramID)
    {
        return juce::roundToInt (apvts.getRawParameterValue (paramID)->load());
    }

    // Pontos das formas prontas, usados como ponto de partida no editor (RF15).
    ShapePoints lfoPresetPoints (LfoShape shape)
    {
        using P = ShapePoints::Preset;
        switch (shape)
        {
            case LfoShape::sine:     return ShapePoints::preset (P::sine);
            case LfoShape::triangle: return ShapePoints::preset (P::triangleUp);
            case LfoShape::sawUp:    return ShapePoints::preset (P::sawUp);
            case LfoShape::sawDown:  return ShapePoints::preset (P::sawDown);
            case LfoShape::square:   return ShapePoints::preset (P::square);
            default:                 return ShapePoints::preset (P::flat);
        }
    }

    ShapePoints oscPresetPoints (Waveform wave)
    {
        using P = ShapePoints::Preset;
        switch (wave)
        {
            case Waveform::sine:     return ShapePoints::preset (P::sine);
            case Waveform::saw:      return ShapePoints::preset (P::sawUp);
            case Waveform::square:   return ShapePoints::preset (P::square);
            case Waveform::triangle: return ShapePoints::preset (P::triangleDown);
            default:                 return ShapePoints::preset (P::sawUp);
        }
    }
}

// ----------------------------------------------------------------------------- TabPage

void TabPage::addRow (std::initializer_list<ParameterGroup*> groups, int weight, int rowMaxHeight)
{
    rows.push_back ({ groups, weight, rowMaxHeight });

    for (auto* group : groups)
        addAndMakeVisible (*group);
}

void TabPage::setNote (const juce::String& text)
{
    note.setText (text, juce::dontSendNotification);
    note.setJustificationType (juce::Justification::topLeft);
    note.setFont (juce::FontOptions (13.0f));
    note.setColour (juce::Label::textColourId,
                    getLookAndFeel().findColour (juce::Label::textColourId).withAlpha (0.6f));
    addAndMakeVisible (note);
}

void TabPage::resized()
{
    auto area = getLocalBounds().reduced (margin);

    if (noteHeight > 0)
        note.setBounds (area.removeFromBottom (noteHeight));

    if (rows.empty())
        return;

    const int numRows = static_cast<int> (rows.size());
    int totalWeight = 0;
    for (const auto& row : rows)
        totalWeight += row.weight;

    // Altura de uma linha de peso 1.
    const int unitHeight = (area.getHeight() - gap * (numRows - 1)) / totalWeight;

    // Linhas com altura máxima própria liberam espaço para as linhas sem limite próprio.
    int freedHeight = 0, uncappedWeight = 0;
    for (const auto& row : rows)
    {
        if (row.maxHeight > 0)
            freedHeight += juce::jmax (0, unitHeight * row.weight - row.maxHeight);
        else
            uncappedWeight += row.weight;
    }

    for (const auto& row : rows)
    {
        int height = unitHeight * row.weight;
        if (row.maxHeight > 0)
            height = juce::jmin (height, row.maxHeight);
        else if (uncappedWeight > 0)
            height += freedHeight * row.weight / uncappedWeight;

        layoutRow (area.removeFromTop (juce::jmin (maxRowHeight, height)), row.groups);
        area.removeFromTop (gap);
    }

    if (noteHeight == 0)
        note.setBounds (area); // a nota ocupa o espaço que sobrar embaixo
}

// ----------------------------------------------------------------------------- PluginEditor

PluginEditor::PluginEditor (PluginProcessor& processor)
    : AudioProcessorEditor (processor),
      pluginProcessor (processor),
      modulationModel (processor.apvts)
{
    setLookAndFeel (&lookAndFeel);

    auto& apvts = processor.apvts;

    // --- Aba Oscillators (RF6) ---
    for (size_t i = 0; i < oscillatorGroups.size(); ++i)
    {
        auto& group = oscillatorGroups[i];
        group.add (std::make_unique<Toggle> (apvts, ParamIDs::oscOn[i],     "On"));
        group.add (std::make_unique<Choice> (apvts, ParamIDs::oscWave[i],   "Waveform"));
        group.add (std::make_unique<OscillatorGraph> (
            apvts, i,
            [this, i] { return pluginProcessor.getOscPoints (i); },
            [this, i] (juce::Component& target) { openOscShapeEditor (i, target); }));
        group.add (std::make_unique<Knob>   (apvts, ParamIDs::oscLevel[i],  "Level"))
            .enableModulation (modulationModel, ModMatrix::oscLevelDest (i));
        group.add (std::make_unique<Knob>   (apvts, ParamIDs::oscOctave[i], "Octave"));
        // Pitch do oscilador no knob de Steps: 100 % = 24 passos, metade da faixa de 48.
        group.add (std::make_unique<Knob>   (apvts, ParamIDs::oscSteps[i],  "Steps"))
            .enableModulation (modulationModel, ModMatrix::oscPitchDest (i), 0.5f);
        group.add (std::make_unique<Knob>   (apvts, ParamIDs::oscFine[i],   "Fine"))
            .enableModulation (modulationModel, ModMatrix::oscFineDest (i));

        // Unison (RF12)
        group.add (std::make_unique<Knob>   (apvts, ParamIDs::oscUnison[i], "Unison"));
        group.add (std::make_unique<Knob>   (apvts, ParamIDs::oscDetune[i], "Detune"));
        group.add (std::make_unique<Knob>   (apvts, ParamIDs::oscBlend[i],  "Blend"));
        group.add (std::make_unique<Knob>   (apvts, ParamIDs::oscWidth[i],  "Width"));

        oscillatorsPage.addRow ({ &group });
    }

    // --- Aba Amp & Filter ---
    voiceGroup.add (std::make_unique<Choice> (apvts, ParamIDs::voiceMode, "Mode"));
    voiceGroup.add (std::make_unique<Toggle> (apvts, ParamIDs::legato,    "Legato"));
    voiceGroup.add (std::make_unique<Knob>   (apvts, ParamIDs::glide,     "Glide"));

    outputGroup.add (std::make_unique<Knob> (apvts, ParamIDs::gain, "Volume"));

    // Envelope de volume e de modulação em abas internas, cada um com gráfico e 4 knobs.
    envelopeGroup.add (std::make_unique<EnvelopeTabs> (apvts, processor.envelopeDisplay, processor.modEnvDisplay));
    envelopeGroup.setLayoutWeight (4); // ocupa a largura de 4 knobs, como antes

    filterGroup.add (std::make_unique<Knob> (apvts, ParamIDs::cutoff, "Cutoff"))
        .enableModulation (modulationModel, ModMatrix::Dest::cutoff);
    filterGroup.add (std::make_unique<Knob> (apvts, ParamIDs::reso,   "Resonance"))
        .enableModulation (modulationModel, ModMatrix::Dest::resonance);

    // Gráfico (RF9) acima dos knobs do filtro (os do envelope ficam dentro das abas).
    filterGroup.setDisplay (std::make_unique<FilterGraph> (apvts));

    ampFilterPage.addRow ({ &voiceGroup, &outputGroup });
    ampFilterPage.addRow ({ &envelopeGroup, &filterGroup }, 2); // linha com gráficos: o dobro da altura

    // --- Aba Matrix (RF10) --- só os 8 slots (o Mod Env fica nas abas do Envelope).
    slotsGroup.add (std::make_unique<ModSlotList> (apvts, modulationModel));
    matrixPage.addRow ({ &slotsGroup });
    matrixPage.setMaxRowHeight (360); // linhas dos slots sem esticar demais em janelas altas

    // --- Aba LFOs (RF11) ---
    for (size_t k = 0; k < lfoGroups.size(); ++k)
    {
        auto& group = lfoGroups[k];
        auto& controls = lfoSyncControls[k];

        group.add (std::make_unique<Choice> (apvts, ParamIDs::lfoShape[k], "Shape"));
        controls.rate = &group.add (std::make_unique<Knob> (apvts, ParamIDs::lfoRate[k], "Rate"));
        group.add (std::make_unique<Toggle> (apvts, ParamIDs::lfoSync[k], "Sync"));
        controls.division = &group.add (std::make_unique<Choice> (apvts, ParamIDs::lfoDivision[k], "Division"));
        group.add (std::make_unique<Choice> (apvts, ParamIDs::lfoTrigger[k], "Trigger"));

        // Editor da forma (RF15) à esquerda: o grupo é largo e baixo, então os controles
        // ficam com a altura toda e o knob Rate não encolhe.
        group.setDisplay (makeLfoShapeEditor (k), 0.55f, ParameterGroup::DisplayPlacement::left);

        if (auto* syncParam = apvts.getParameter (ParamIDs::lfoSync[k]))
        {
            controls.syncAttachment = std::make_unique<juce::ParameterAttachment> (
                *syncParam, [this, k] (float value) { setLfoSynced (k, value > 0.5f); });
            controls.syncAttachment->sendInitialUpdate();
        }

        lfoPage.addRow ({ &group });
    }

    // --- Fontes arrastáveis (cabeçalho) ---
    for (auto source : { ModMatrix::Source::velocity, ModMatrix::Source::modWheel, ModMatrix::Source::aftertouch,
                         ModMatrix::Source::keyTrack, ModMatrix::Source::modEnv,
                         ModMatrix::Source::lfo1, ModMatrix::Source::lfo2, ModMatrix::Source::lfo3 })
    {
        sourceChips.push_back (std::make_unique<SourceChip> (source));
        addAndMakeVisible (*sourceChips.back());
    }

    // --- Aba Tuning (RF8) ---
    tuningGroup.add (std::make_unique<Toggle> (apvts, ParamIDs::tuningLock, "Lock"));
    tuningControls.push_back (&tuningGroup.add (std::make_unique<Knob> (apvts, ParamIDs::edo,       "EDO")));
    tuningControls.push_back (&tuningGroup.add (std::make_unique<Knob> (apvts, ParamIDs::baseHz,    "Base Freq")));
    tuningControls.push_back (&tuningGroup.add (std::make_unique<Knob> (apvts, ParamIDs::refNote,   "Ref Note")));
    tuningControls.push_back (&tuningGroup.add (std::make_unique<Knob> (apvts, ParamIDs::bendSteps, "Bend Range")));

    // Gráfico dos steps da EDO (RF16): ocupa o grupo inteiro (sem controles próprios).
    stepsGroup.setDisplay (std::make_unique<TuningStepsView> (processor), 1.0f, ParameterGroup::DisplayPlacement::left);
    stepsGroup.setLayoutWeight (1); // sem controles, o peso seria 0 e o grupo ficaria sem largura

    tuningPage.addRow ({ &tuningGroup }, 1, 170);
    tuningPage.addRow ({ &stepsGroup }, 2);
    tuningPage.setNoteHeight (36);
    tuningPage.setNote ("Lock protects the tuning knobs from accidental changes in this window. "
                        "Host automation still applies.\n"
                        "Tuning changes affect the next notes played; held notes keep their pitch.");

    // --- Abas ---
    const auto tabColour = lookAndFeel.findColour (juce::ResizableWindow::backgroundColourId);
    tabs.setTabBarDepth (tabBarDepth);
    tabs.setOutline (0);
    // false = as abas não apagam as páginas (elas são membros do editor).
    tabs.addTab ("Oscillators",  tabColour, &oscillatorsPage, false);
    tabs.addTab ("Amp & Filter", tabColour, &ampFilterPage,   false);
    tabs.addTab ("Matrix",       tabColour, &matrixPage,      false);
    tabs.addTab ("LFOs",         tabColour, &lfoPage,         false);
    tabs.addTab ("Tuning",       tabColour, &tuningPage,      false);
    addAndMakeVisible (tabs);

    // Trava: o callback recebe o valor do parâmetro (0 ou 1) quando ele muda, inclusive ao
    // carregar um set. sendInitialUpdate aplica o valor atual já na abertura da janela.
    if (auto* lockParam = apvts.getParameter (ParamIDs::tuningLock))
    {
        tuningLockAttachment = std::make_unique<juce::ParameterAttachment> (
            *lockParam, [this] (float value) { setTuningLocked (value > 0.5f); });
        tuningLockAttachment->sendInitialUpdate();
    }

    setResizable (true, true); // true, true = pode redimensionar e mostra a alça no canto
    setResizeLimits (minWidth, minHeight, maxWidth, maxHeight);
    setSize (defaultWidth, defaultHeight);
}

PluginEditor::~PluginEditor()
{
    // Desligar o LookAndFeel antes que ele (um membro) seja destruído.
    setLookAndFeel (nullptr);
}

void PluginEditor::setTuningLocked (bool locked)
{
    for (auto* control : tuningControls)
    {
        control->setEnabled (! locked);
        control->setAlpha (locked ? 0.4f : 1.0f); // apagado = travado
    }
}

void PluginEditor::setLfoSynced (size_t lfoIndex, bool synced)
{
    auto& controls = lfoSyncControls[lfoIndex];

    // Antes do primeiro sendInitialUpdate os ponteiros já existem (são preenchidos antes).
    controls.rate->setEnabled (! synced);
    controls.rate->setAlpha (synced ? 0.4f : 1.0f);
    controls.division->setEnabled (synced);
    controls.division->setAlpha (synced ? 1.0f : 0.4f);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    auto header = getLocalBounds().reduced (margin, 0).removeFromTop (headerHeight + margin / 2);

    g.setColour (getLookAndFeel().findColour (juce::Label::textColourId));
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("Matiza", header, juce::Justification::centredLeft);

    g.setColour (getLookAndFeel().findColour (juce::Label::textColourId).withAlpha (0.5f));
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("v" JucePlugin_VersionString, header, juce::Justification::centredRight);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (headerHeight + margin / 2);
    tabs.setBounds (area);

    // Alças das fontes entre o nome e a versão, divididas igualmente.
    auto chipsArea = header.reduced (margin, 0);
    chipsArea.removeFromLeft (titleWidth);
    chipsArea.removeFromRight (versionWidth);
    chipsArea = chipsArea.withSizeKeepingCentre (chipsArea.getWidth(), 24);

    const int numChips = static_cast<int> (sourceChips.size());
    const int chipWidth = juce::jmin (90, (chipsArea.getWidth() - 4 * (numChips - 1)) / numChips);
    for (auto& chip : sourceChips)
    {
        chip->setBounds (chipsArea.removeFromLeft (chipWidth));
        chipsArea.removeFromLeft (4);
    }
}

// ----------------------------------------------------------------------------- Formas desenhadas (RF15)

std::unique_ptr<ShapeEditor> PluginEditor::makeLfoShapeEditor (std::size_t lfo)
{
    auto editor = std::make_unique<ShapeEditor>();
    auto& apvts = pluginProcessor.apvts;
    const char* shapeID = ParamIDs::lfoShape[lfo];

    // Mostra a forma desenhada (Custom) ou os pontos da forma pronta escolhida.
    editor->getPoints = [this, &apvts, shapeID, lfo]
    {
        const auto shape = static_cast<LfoShape> (choiceIndex (apvts, shapeID));
        return shape == LfoShape::custom ? pluginProcessor.getLfoPoints (lfo) : lfoPresetPoints (shape);
    };

    // A primeira edição de uma forma pronta parte dos pontos dela e liga o Custom.
    editor->onEdit = [this, &apvts, shapeID, lfo] (const ShapePoints& points)
    {
        pluginProcessor.setLfoPoints (lfo, points);
        setChoiceIndex (apvts, shapeID, static_cast<int> (LfoShape::custom));
    };

    editor->isEditable = [&apvts, shapeID]
    {
        return static_cast<LfoShape> (choiceIndex (apvts, shapeID)) != LfoShape::sampleHold;
    };
    editor->notEditableMessage = "Sample & Hold: a new random value every cycle (not drawable)";

    editor->getPlayhead = [this, lfo] { return pluginProcessor.lfoDisplayPhase[lfo].load(); };
    return editor;
}

std::unique_ptr<ShapeEditor> PluginEditor::makeOscShapeEditor (std::size_t osc)
{
    auto editor = std::make_unique<ShapeEditor>();
    auto& apvts = pluginProcessor.apvts;
    const char* waveID = ParamIDs::oscWave[osc];

    editor->getPoints = [this, &apvts, waveID, osc]
    {
        const auto wave = static_cast<Waveform> (choiceIndex (apvts, waveID));
        return wave == Waveform::custom ? pluginProcessor.getOscPoints (osc) : oscPresetPoints (wave);
    };

    editor->onEdit = [this, &apvts, waveID, osc] (const ShapePoints& points)
    {
        pluginProcessor.setOscPoints (osc, points);
        setChoiceIndex (apvts, waveID, static_cast<int> (Waveform::custom));
    };

    return editor;
}

void PluginEditor::openOscShapeEditor (std::size_t osc, juce::Component& target)
{
    auto editor = makeOscShapeEditor (osc);
    editor->setSize (juce::jmin (520, getWidth() - 40), juce::jmin (280, getHeight() - 80));

    // Janela flutuante (balão) dentro da janela do plugin, apontando para o gráfico clicado.
    // Fecha sozinha ao clicar fora. O CallOutBox vira dono do editor.
    juce::CallOutBox::launchAsynchronously (std::move (editor), getLocalArea (&target, target.getLocalBounds()), this);
}
