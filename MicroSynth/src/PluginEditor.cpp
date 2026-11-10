#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Graphs.h"

namespace
{
    constexpr int headerHeight = 36;
    constexpr int margin       = 12;
    constexpr int gap          = 10;
    constexpr int tabBarDepth  = 30;

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
    : AudioProcessorEditor (processor)
{
    setLookAndFeel (&lookAndFeel);

    auto& apvts = processor.apvts;

    // --- Aba Oscillators (RF6) ---
    for (size_t i = 0; i < oscillatorGroups.size(); ++i)
    {
        auto& group = oscillatorGroups[i];
        group.add (std::make_unique<Toggle> (apvts, ParamIDs::oscOn[i],     "On"));
        group.add (std::make_unique<Choice> (apvts, ParamIDs::oscWave[i],   "Waveform"));
        group.add (std::make_unique<Knob>   (apvts, ParamIDs::oscLevel[i],  "Level"));
        group.add (std::make_unique<Knob>   (apvts, ParamIDs::oscOctave[i], "Octave"));
        group.add (std::make_unique<Knob>   (apvts, ParamIDs::oscSteps[i],  "Steps"));
        group.add (std::make_unique<Knob>   (apvts, ParamIDs::oscFine[i],   "Fine"));

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

    filterGroup.add (std::make_unique<Knob> (apvts, ParamIDs::cutoff, "Cutoff"));
    filterGroup.add (std::make_unique<Knob> (apvts, ParamIDs::reso,   "Resonance"));

    // Gráficos (RF9) acima dos knobs do envelope e do filtro.
    envelopeGroup.setDisplay (std::make_unique<EnvelopeGraph> (apvts, processor.envelopeDisplay));
    filterGroup.setDisplay (std::make_unique<FilterGraph> (apvts));

    ampFilterPage.addRow ({ &voiceGroup, &outputGroup });
    ampFilterPage.addRow ({ &envelopeGroup, &filterGroup }, 2); // linha com gráficos: o dobro da altura

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
    area.removeFromTop (headerHeight + margin / 2);
    tabs.setBounds (area);
}
