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
    bendStepsParam = apvts.getRawParameterValue (ParamIDs::bendSteps);
    waveParam    = apvts.getRawParameterValue (ParamIDs::wave);
    attackParam  = apvts.getRawParameterValue (ParamIDs::attack);
    decayParam   = apvts.getRawParameterValue (ParamIDs::decay);
    sustainParam = apvts.getRawParameterValue (ParamIDs::sustain);
    releaseParam = apvts.getRawParameterValue (ParamIDs::release);
    cutoffParam  = apvts.getRawParameterValue (ParamIDs::cutoff);
    resoParam    = apvts.getRawParameterValue (ParamIDs::reso);
    gainParam    = apvts.getRawParameterValue (ParamIDs::gain);

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

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ParamIDs::bendSteps, 1 }, "Pitch Bend Range", 0, 24, 2,
        juce::AudioParameterIntAttributes().withLabel ("steps")));

    // --- Oscilador --- (a ordem das opções = enum Waveform)
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::wave, 1 }, "Waveform",
        juce::StringArray { "Sine", "Saw", "Square", "Triangle" }, 1));

    // --- Envelope --- tempos em ms; o skew dá mais resolução aos valores curtos.
    auto timeRange = [] (float maxMs, float centreMs)
    {
        juce::NormalisableRange<float> range (1.0f, maxMs, 0.1f);
        range.setSkewForCentre (centreMs);
        return range;
    };
    const auto msAttributes = juce::AudioParameterFloatAttributes().withLabel ("ms");

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::attack, 1 }, "Attack", timeRange (5000.0f, 200.0f), 5.0f, msAttributes));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::decay, 1 }, "Decay", timeRange (5000.0f, 300.0f), 200.0f, msAttributes));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::sustain, 1 }, "Sustain",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::release, 1 }, "Release", timeRange (10000.0f, 500.0f), 300.0f, msAttributes));

    // --- Filtro --- centro em 1 kHz aproxima uma escala logarítmica, como o ouvido percebe.
    juce::NormalisableRange<float> cutoffRange (20.0f, 20000.0f, 0.1f);
    cutoffRange.setSkewForCentre (1000.0f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::cutoff, 1 }, "Cutoff", cutoffRange, 8000.0f,
        juce::AudioParameterFloatAttributes().withLabel ("Hz")));

    juce::NormalisableRange<float> resoRange (0.1f, 10.0f, 0.01f);
    resoRange.setSkewForCentre (1.0f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::reso, 1 }, "Resonance", resoRange, 0.7f));

    // --- Saída ---
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::gain, 1 }, "Volume",
        juce::NormalisableRange<float> (-48.0f, 6.0f, 0.1f), -12.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return layout;
}

void PluginProcessor::updateTuning()
{
    TuningEngine::Settings settings = tuning.getSettings();
    settings.edo     = static_cast<int> (edoParam->load());
    settings.baseHz  = baseHzParam->load();
    settings.refNote = static_cast<int> (refNoteParam->load());
    settings.bendSteps = bendStepsParam->load();
    tuning.setSettings (settings);
}

void PluginProcessor::updateVoices()
{
    VoiceParameters params;
    params.waveform     = static_cast<Waveform> (static_cast<int> (waveParam->load()));
    params.adsr.attack  = attackParam->load() / 1000.0f; // ms -> s
    params.adsr.decay   = decayParam->load() / 1000.0f;
    params.adsr.sustain = sustainParam->load();
    params.adsr.release = releaseParam->load() / 1000.0f;
    params.cutoffHz     = cutoffParam->load();
    params.resonance    = resoParam->load();

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<MicroVoice*> (synth.getVoice (i)))
            voice->setParameters (params);
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<MicroVoice*> (synth.getVoice (i)))
            voice->prepare (sampleRate, samplesPerBlock);

    updateTuning();
    updateVoices();

    gainSmoothed.reset (sampleRate, 0.02);
    gainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (gainParam->load()));
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

    // Onda, envelope e filtro valem em tempo real, inclusive para notas presas.
    updateVoices();

    const int numSamples = buffer.getNumSamples();
    synth.renderNextBlock (buffer, midi, 0, numSamples);

    // Volume de saída, suavizado amostra a amostra para não estalar.
    gainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (gainParam->load()));

    for (int s = 0; s < numSamples; ++s)
    {
        const float g = gainSmoothed.getNextValue();

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.getWritePointer (ch)[s] *= g;
    }
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
