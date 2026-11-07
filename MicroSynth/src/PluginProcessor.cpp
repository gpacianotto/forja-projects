#include "PluginProcessor.h"
#include "MicroSound.h"
#include "MicroVoice.h"
#include "Parameters.h"

PluginProcessor::PluginProcessor()
    // Instrumento: sem barramento de entrada, só saída estéreo.
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    edoParam     = apvts.getRawParameterValue (ParamIDs::edo);
    baseHzParam  = apvts.getRawParameterValue (ParamIDs::baseHz);
    refNoteParam = apvts.getRawParameterValue (ParamIDs::refNote);

    // O Synthesiser vira dono dos ponteiros (como um "new" cujo delete fica a cargo dele).
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new MicroVoice (tuning));

    synth.addSound (new MicroSound());
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // --- Afinação ---
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParamIDs::edo, 1 }, "Divisions per Octave", 1, 72, 12));

    // Faixa 20–2000 Hz com o meio do slider em 440 Hz (o valor mais usado).
    juce::NormalisableRange<float> baseHzRange (20.0f, 2000.0f, 0.01f);
    baseHzRange.setSkewForCentre (440.0f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::baseHz, 1 }, "Base Frequency", baseHzRange, 440.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParamIDs::refNote, 1 }, "Reference Note", 0, 127, 69));

    return layout;
}

void PluginProcessor::updateTuning()
{
    TuningEngine::Settings settings = tuning.getSettings();
    settings.edo     = static_cast<int> (edoParam->load());
    settings.baseHz  = baseHzParam->load();
    settings.refNote = static_cast<int> (refNoteParam->load());
    tuning.setSettings (settings);
}

void PluginProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<MicroVoice*> (synth.getVoice (i)))
            voice->prepare (sampleRate);
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // O buffer chega com lixo/silêncio; as vozes somam por cima dele.
    buffer.clear();

    // RF3: a afinação nova vale para as próximas notas; notas presas mantêm a frequência.
    updateTuning();

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    // Interface genérica: um slider por parâmetro. Interface própria na etapa 6.
    return new juce::GenericAudioProcessorEditor (*this);
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// Ponto de entrada que o host chama para criar o plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
