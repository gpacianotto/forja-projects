#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Graphs.h"

namespace
{
    constexpr int headerHeight = 36;
    constexpr int margin       = 12;
    constexpr int gap          = 10;
    constexpr int tabBarDepth  = 30;
    constexpr int titleWidth   = 140; // espaço do nome "MicroSynth" no cabeçalho
    constexpr int versionWidth = 60;

    // Tamanho inicial e limites da janela redimensionável.
    constexpr int defaultWidth  = 840, defaultHeight = 560;
    constexpr int minWidth      = 640, minHeight     = 460;
    constexpr int maxWidth      = 1680, maxHeight    = 1120;

    // Uma linha de grupos; cada grupo ganha largura proporcional ao seu número de controles,
    // para que todos os knobs da linha tenham o mesmo tamanho.
    void layoutRow (juce::Rectangle<int> area, const std::vector<ParameterGroup*>& groups)
    {
        juce::FlexBox row;
        row.flexDirection = juce::FlexBox::Direction::row;

        for (auto* group : groups)
            row.items.add (juce::FlexItem (*group)
                               .withFlex (static_cast<float> (group->getNumControls()))
                               .withMargin (juce::FlexItem::Margin (0.0f, gap / 2.0f, 0.0f, gap / 2.0f)));

        row.performLayout (area.expanded (gap / 2, 0));
    }
}

// ----------------------------------------------------------------------------- TabPage

void TabPage::addRow (std::initializer_list<ParameterGroup*> groups, int weight)
{
    rows.push_back ({ groups, weight });

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

    if (rows.empty())
        return;

    const int numRows = static_cast<int> (rows.size());
    int totalWeight = 0;
    for (const auto& row : rows)
        totalWeight += row.weight;

    // Altura de uma linha de peso 1.
    const int unitHeight = (area.getHeight() - gap * (numRows - 1)) / totalWeight;

    for (const auto& row : rows)
    {
        layoutRow (area.removeFromTop (juce::jmin (maxRowHeight, unitHeight * row.weight)), row.groups);
        area.removeFromTop (gap);
    }

    note.setBounds (area); // a nota ocupa o espaço que sobrar embaixo
}

// ----------------------------------------------------------------------------- PluginEditor

PluginEditor::PluginEditor (PluginProcessor& processor)
    : AudioProcessorEditor (processor),
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
        group.add (std::make_unique<Knob>   (apvts, ParamIDs::oscLevel[i],  "Level"))
            .enableModulation (modulationModel, ModMatrix::oscLevelDest (i));
        group.add (std::make_unique<Knob>   (apvts, ParamIDs::oscOctave[i], "Octave"));
        // Pitch do oscilador no knob de Steps: 100 % = 24 passos, metade da faixa de 48.
        group.add (std::make_unique<Knob>   (apvts, ParamIDs::oscSteps[i],  "Steps"))
            .enableModulation (modulationModel, ModMatrix::oscPitchDest (i), 0.5f);
        group.add (std::make_unique<Knob>   (apvts, ParamIDs::oscFine[i],   "Fine"))
            .enableModulation (modulationModel, ModMatrix::oscFineDest (i));

        oscillatorsPage.addRow ({ &group });
    }

    // --- Aba Amp & Filter ---
    voiceGroup.add (std::make_unique<Choice> (apvts, ParamIDs::voiceMode, "Mode"));
    voiceGroup.add (std::make_unique<Toggle> (apvts, ParamIDs::legato,    "Legato"));
    voiceGroup.add (std::make_unique<Knob>   (apvts, ParamIDs::glide,     "Glide"));

    outputGroup.add (std::make_unique<Knob> (apvts, ParamIDs::gain, "Volume"));

    envelopeGroup.add (std::make_unique<Knob> (apvts, ParamIDs::attack,  "Attack"));
    envelopeGroup.add (std::make_unique<Knob> (apvts, ParamIDs::decay,   "Decay"));
    envelopeGroup.add (std::make_unique<Knob> (apvts, ParamIDs::sustain, "Sustain"));
    envelopeGroup.add (std::make_unique<Knob> (apvts, ParamIDs::release, "Release"));

    filterGroup.add (std::make_unique<Knob> (apvts, ParamIDs::cutoff, "Cutoff"))
        .enableModulation (modulationModel, ModMatrix::Dest::cutoff);
    filterGroup.add (std::make_unique<Knob> (apvts, ParamIDs::reso,   "Resonance"))
        .enableModulation (modulationModel, ModMatrix::Dest::resonance);

    // Gráficos (RF9) acima dos knobs do envelope e do filtro.
    envelopeGroup.setDisplay (std::make_unique<EnvelopeGraph> (
        apvts, EnvelopeGraph::ParamIDs4 { ParamIDs::attack, ParamIDs::decay, ParamIDs::sustain, ParamIDs::release },
        processor.envelopeDisplay));
    filterGroup.setDisplay (std::make_unique<FilterGraph> (apvts));

    ampFilterPage.addRow ({ &voiceGroup, &outputGroup });
    ampFilterPage.addRow ({ &envelopeGroup, &filterGroup }, 2); // linha com gráficos: o dobro da altura

    // --- Aba Matrix (RF10) ---
    modEnvGroup.add (std::make_unique<Knob> (apvts, ParamIDs::modEnvAttack,  "Attack"));
    modEnvGroup.add (std::make_unique<Knob> (apvts, ParamIDs::modEnvDecay,   "Decay"));
    modEnvGroup.add (std::make_unique<Knob> (apvts, ParamIDs::modEnvSustain, "Sustain"));
    modEnvGroup.add (std::make_unique<Knob> (apvts, ParamIDs::modEnvRelease, "Release"));
    modEnvGroup.setDisplay (std::make_unique<EnvelopeGraph> (
        apvts, EnvelopeGraph::ParamIDs4 { ParamIDs::modEnvAttack, ParamIDs::modEnvDecay,
                                          ParamIDs::modEnvSustain, ParamIDs::modEnvRelease },
        processor.modEnvDisplay),
        0.4f);

    slotsGroup.add (std::make_unique<ModSlotList> (apvts, modulationModel));

    matrixPage.addRow ({ &modEnvGroup });
    matrixPage.addRow ({ &slotsGroup });

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

    tuningPage.addRow ({ &tuningGroup });
    tuningPage.setMaxRowHeight (190);
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
    g.drawText ("MicroSynth", header, juce::Justification::centredLeft);

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
