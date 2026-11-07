#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Controls.h"

class PluginProcessor;

// Interface própria do MicroSynth (etapa 6): knobs agrupados em Tuning, Oscillator, Output,
// Envelope e Filter, numa janela redimensionável.
class PluginEditor final : public juce::AudioProcessorEditor
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Precisa existir enquanto os componentes usarem; por isso é o primeiro membro
    // (construído primeiro, destruído por último).
    juce::LookAndFeel_V4 lookAndFeel { juce::LookAndFeel_V4::getMidnightColourScheme() };

    ParameterGroup tuningGroup     { "Tuning" };
    ParameterGroup oscillatorGroup { "Oscillator" };
    ParameterGroup outputGroup     { "Output" };
    ParameterGroup envelopeGroup   { "Envelope" };
    ParameterGroup filterGroup     { "Filter" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
