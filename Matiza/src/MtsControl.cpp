#include "MtsControl.h"

#include "Palette.h"
#include "Parameters.h"

namespace
{
    constexpr int labelHeight = 18; // igual ao nome dos knobs (Controls.cpp)
}

MtsControl::MtsControl (PluginProcessor& processor)
    : pluginProcessor (processor),
      attachment (processor.apvts, ParamIDs::mtsFollow, followButton)
{
    label.setText ("MTS-ESP", juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (label);

    followButton.setTooltip ("Follow the tuning of an MTS-ESP master (e.g. MTS-ESP Mini) when one is in the session. "
                             "Without a master, the internal tuning applies.");
    followButton.setColour (juce::ToggleButton::tickColourId, Palette::primary);
    addAndMakeVisible (followButton);

    timerCallback(); // primeiro estado já na abertura
    startTimerHz (10);
}

void MtsControl::setOnFollowingChanged (std::function<void (bool)> callback)
{
    onFollowingChanged = std::move (callback);
    if (onFollowingChanged)
        onFollowingChanged (following);
}

void MtsControl::timerCallback()
{
    const auto display = pluginProcessor.getMasterDisplay();
    const juce::String newName = display.active ? juce::String (juce::CharPointer_UTF8 (display.scaleName.data()))
                                                : juce::String();

    const bool followingChanged = display.active != following;
    if (display.hasMaster == hasMaster && ! followingChanged && newName == scaleName)
        return;

    hasMaster = display.hasMaster;
    following = display.active;
    scaleName = newName;
    repaint();

    if (followingChanged && onFollowingChanged)
        onFollowingChanged (following);
}

void MtsControl::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (labelHeight));

    // Caixinha + "Follow" centralizados na coluna.
    area.removeFromTop (4);
    followButton.setBounds (area.removeFromTop (28).withSizeKeepingCentre (76, 28));
    area.removeFromTop (4);
    statusArea = area;
}

void MtsControl::paint (juce::Graphics& g)
{
    auto area = statusArea.toFloat();

    // Linha 1: bolinha + estado.
    auto row = area.removeFromTop (18.0f);
    const juce::String status = following ? "Connected" : (hasMaster ? "Not following" : "No master");

    g.setFont (juce::FontOptions (12.0f));
    const float textWidth = juce::jmin (row.getWidth() - 14.0f,
                                        juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), status) + 2.0f);
    auto content = row.withSizeKeepingCentre (textWidth + 14.0f, row.getHeight());
    const auto dot = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ content.getX() + 4.0f, content.getCentreY() });

    if (following)
    {
        g.setColour (Palette::primary);
        g.fillEllipse (dot);
    }
    else
    {
        g.setColour (Palette::textMuted);
        g.drawEllipse (dot, 1.2f);
    }

    g.setColour (following ? Palette::text : Palette::textMuted);
    g.drawText (status, content.withTrimmedLeft (14.0f), juce::Justification::centredLeft);

    // Linha 2: nome da escala (comprimido se for longo, como o título dos grupos).
    if (following && scaleName.isNotEmpty())
    {
        g.setColour (Palette::textMuted);
        g.setFont (juce::FontOptions (11.0f));
        g.drawFittedText (scaleName, area.removeFromTop (16.0f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
}
