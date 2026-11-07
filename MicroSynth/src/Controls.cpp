#include "Controls.h"

namespace
{
    constexpr int labelHeight = 18;
    constexpr int titleHeight = 24;

    void setupLabel (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (13.0f));
    }
}

// ----------------------------------------------------------------------------- Knob

Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& labelText)
    // O slider precisa existir antes do attachment: a lista de inicialização segue a ordem
    // de declaração no .h (label, slider, attachment).
    : attachment (apvts, paramID, slider)
{
    setupLabel (label, labelText);
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, labelHeight);

    if (auto* param = apvts.getParameter (paramID))
    {
        // Duplo clique volta ao valor padrão do parâmetro.
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

        // Unidade do parâmetro (Hz, ms, dB...) depois do número.
        if (param->getLabel().isNotEmpty())
            slider.setTextValueSuffix (" " + param->getLabel());
    }

    addAndMakeVisible (slider);
}

void Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (labelHeight));
    slider.setBounds (area);
}

// ----------------------------------------------------------------------------- Choice

Choice::Choice (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& labelText)
{
    setupLabel (label, labelText);
    addAndMakeVisible (label);

    // As opções vêm do próprio parâmetro, para nunca ficarem diferentes do que o host vê.
    // Precisam estar na lista antes de criar o attachment.
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramID)))
        comboBox.addItemList (choiceParam->choices, 1);

    comboBox.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (comboBox);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramID, comboBox);
}

void Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (labelHeight));

    // Combo com altura fixa, centralizado verticalmente no espaço que sobra.
    const int comboHeight = 26;
    comboBox.setBounds (area.withSizeKeepingCentre (juce::jmin (area.getWidth() - 8, 140), comboHeight));
}

// ----------------------------------------------------------------------------- ParameterGroup

ParameterGroup::ParameterGroup (const juce::String& groupTitle) : title (groupTitle) {}

void ParameterGroup::add (std::unique_ptr<juce::Component> control)
{
    addAndMakeVisible (*control);
    controls.push_back (std::move (control)); // std::move: transfere a posse, não copia
}

void ParameterGroup::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const auto& lf = getLookAndFeel();

    g.setColour (lf.findColour (juce::ResizableWindow::backgroundColourId).brighter (0.08f));
    g.fillRoundedRectangle (bounds, 8.0f);

    g.setColour (lf.findColour (juce::Slider::rotarySliderFillColourId).withAlpha (0.5f));
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

    g.setColour (lf.findColour (juce::Label::textColourId));
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    // drawFittedText comprime o texto (até 70% da largura) em vez de cortá-lo com "..."
    // quando a janela está estreita.
    g.drawFittedText (title.toUpperCase(), getLocalBounds().removeFromTop (titleHeight).reduced (8, 0),
                      juce::Justification::centredLeft, 1, 0.7f);
}

void ParameterGroup::resized()
{
    auto area = getLocalBounds().reduced (6);
    area.removeFromTop (titleHeight - 6);

    // FlexBox: como o flexbox do CSS. Cada controle ocupa uma fatia igual da largura.
    juce::FlexBox row;
    row.flexDirection = juce::FlexBox::Direction::row;

    for (auto& control : controls)
        row.items.add (juce::FlexItem (*control).withFlex (1.0f).withMargin (2.0f));

    row.performLayout (area);
}
