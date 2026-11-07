#include "PluginEditor.h"
#include "Parameters.h"
#include "PluginProcessor.h"

namespace
{
    constexpr int headerHeight = 36;
    constexpr int margin       = 12;
    constexpr int gap          = 10;

    // Tamanho inicial e limites da janela redimensionável.
    constexpr int defaultWidth  = 840, defaultHeight = 480;
    constexpr int minWidth      = 640, minHeight     = 380;
    constexpr int maxWidth      = 1680, maxHeight    = 960;

    // Uma linha de grupos; cada grupo ganha largura proporcional ao seu número de controles,
    // para que todos os knobs tenham o mesmo tamanho.
    void layoutRow (juce::Rectangle<int> area, std::initializer_list<ParameterGroup*> groups)
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

PluginEditor::PluginEditor (PluginProcessor& processor)
    : AudioProcessorEditor (processor)
{
    setLookAndFeel (&lookAndFeel);

    auto& apvts = processor.apvts;

    tuningGroup.add (std::make_unique<Knob> (apvts, ParamIDs::edo,       "EDO"));
    tuningGroup.add (std::make_unique<Knob> (apvts, ParamIDs::baseHz,    "Base Freq"));
    tuningGroup.add (std::make_unique<Knob> (apvts, ParamIDs::refNote,   "Ref Note"));
    tuningGroup.add (std::make_unique<Knob> (apvts, ParamIDs::bendSteps, "Bend Range"));

    oscillatorGroup.add (std::make_unique<Choice> (apvts, ParamIDs::wave, "Waveform"));

    outputGroup.add (std::make_unique<Knob> (apvts, ParamIDs::gain, "Volume"));

    envelopeGroup.add (std::make_unique<Knob> (apvts, ParamIDs::attack,  "Attack"));
    envelopeGroup.add (std::make_unique<Knob> (apvts, ParamIDs::decay,   "Decay"));
    envelopeGroup.add (std::make_unique<Knob> (apvts, ParamIDs::sustain, "Sustain"));
    envelopeGroup.add (std::make_unique<Knob> (apvts, ParamIDs::release, "Release"));

    filterGroup.add (std::make_unique<Knob> (apvts, ParamIDs::cutoff, "Cutoff"));
    filterGroup.add (std::make_unique<Knob> (apvts, ParamIDs::reso,   "Resonance"));

    for (auto* group : { &tuningGroup, &oscillatorGroup, &outputGroup, &envelopeGroup, &filterGroup })
        addAndMakeVisible (*group);

    setResizable (true, true); // true, true = pode redimensionar e mostra a alça no canto
    setResizeLimits (minWidth, minHeight, maxWidth, maxHeight);
    setSize (defaultWidth, defaultHeight);
}

PluginEditor::~PluginEditor()
{
    // Desligar o LookAndFeel antes que ele (um membro) seja destruído.
    setLookAndFeel (nullptr);
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
    auto area = getLocalBounds().reduced (margin);
    area.removeFromTop (headerHeight);

    const int rowHeight = (area.getHeight() - gap) / 2;
    auto topRow = area.removeFromTop (rowHeight);
    area.removeFromTop (gap);
    auto bottomRow = area;

    // 6 "colunas" de controles por linha: Tuning(4) + Oscillator(1) + Output(1)
    //                                     Envelope(4) + Filter(2)
    layoutRow (topRow,    { &tuningGroup, &oscillatorGroup, &outputGroup });
    layoutRow (bottomRow, { &envelopeGroup, &filterGroup });
}
