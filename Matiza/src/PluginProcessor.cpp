#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "MicroSound.h"
#include "MicroVoice.h"
#include "Parameters.h"

PluginProcessor::PluginProcessor()
    // Instrumento: sem barramento de entrada, só saída estéreo.
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    edoParam       = apvts.getRawParameterValue (ParamIDs::edo);
    baseHzParam    = apvts.getRawParameterValue (ParamIDs::baseHz);
    refNoteParam   = apvts.getRawParameterValue (ParamIDs::refNote);
    bendStepsParam = apvts.getRawParameterValue (ParamIDs::bendSteps);
    attackParam    = apvts.getRawParameterValue (ParamIDs::attack);
    decayParam     = apvts.getRawParameterValue (ParamIDs::decay);
    sustainParam   = apvts.getRawParameterValue (ParamIDs::sustain);
    releaseParam   = apvts.getRawParameterValue (ParamIDs::release);
    cutoffParam    = apvts.getRawParameterValue (ParamIDs::cutoff);
    resoParam      = apvts.getRawParameterValue (ParamIDs::reso);
    gainParam      = apvts.getRawParameterValue (ParamIDs::gain);
    voiceModeParam = apvts.getRawParameterValue (ParamIDs::voiceMode);
    legatoParam    = apvts.getRawParameterValue (ParamIDs::legato);
    glideParam     = apvts.getRawParameterValue (ParamIDs::glide);

    for (size_t i = 0; i < oscParams.size(); ++i)
    {
        oscParams[i].on     = apvts.getRawParameterValue (ParamIDs::oscOn[i]);
        oscParams[i].wave   = apvts.getRawParameterValue (ParamIDs::oscWave[i]);
        oscParams[i].level  = apvts.getRawParameterValue (ParamIDs::oscLevel[i]);
        oscParams[i].octave = apvts.getRawParameterValue (ParamIDs::oscOctave[i]);
        oscParams[i].steps  = apvts.getRawParameterValue (ParamIDs::oscSteps[i]);
        oscParams[i].fine   = apvts.getRawParameterValue (ParamIDs::oscFine[i]);
        oscParams[i].unison = apvts.getRawParameterValue (ParamIDs::oscUnison[i]);
        oscParams[i].detune = apvts.getRawParameterValue (ParamIDs::oscDetune[i]);
        oscParams[i].blend  = apvts.getRawParameterValue (ParamIDs::oscBlend[i]);
        oscParams[i].width  = apvts.getRawParameterValue (ParamIDs::oscWidth[i]);
    }

    for (size_t i = 0; i < modSlotParams.size(); ++i)
    {
        modSlotParams[i].source = apvts.getRawParameterValue (ParamIDs::modSource[i]);
        modSlotParams[i].dest   = apvts.getRawParameterValue (ParamIDs::modDest[i]);
        modSlotParams[i].amount = apvts.getRawParameterValue (ParamIDs::modAmount[i]);
    }

    for (size_t k = 0; k < lfoParams.size(); ++k)
    {
        lfoParams[k].shape    = apvts.getRawParameterValue (ParamIDs::lfoShape[k]);
        lfoParams[k].rate     = apvts.getRawParameterValue (ParamIDs::lfoRate[k]);
        lfoParams[k].sync     = apvts.getRawParameterValue (ParamIDs::lfoSync[k]);
        lfoParams[k].division = apvts.getRawParameterValue (ParamIDs::lfoDivision[k]);
        lfoParams[k].trigger  = apvts.getRawParameterValue (ParamIDs::lfoTrigger[k]);
    }

    modEnvAttackParam  = apvts.getRawParameterValue (ParamIDs::modEnvAttack);
    modEnvDecayParam   = apvts.getRawParameterValue (ParamIDs::modEnvDecay);
    modEnvSustainParam = apvts.getRawParameterValue (ParamIDs::modEnvSustain);
    modEnvReleaseParam = apvts.getRawParameterValue (ParamIDs::modEnvRelease);

    // A matriz soma na posição normalizada do knob; as vozes usam as mesmas faixas dos parâmetros.
    modRanges.cutoff    = apvts.getParameterRange (ParamIDs::cutoff);
    modRanges.resonance = apvts.getParameterRange (ParamIDs::reso);

    // O Synthesiser vira dono dos ponteiros (como um "new" cujo delete fica a cargo dele).
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new MicroVoice (tuning, globalMod, modRanges));

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

    // Trava da interface (RF8): só a interface lê; não faz sentido automatizar.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::tuningLock, 1 }, "Tuning Lock", false,
        juce::AudioParameterBoolAttributes().withAutomatable (false)));

    // --- Osciladores (RF6) --- (a ordem das opções de onda = enum Waveform)
    // Custom (RF15) no fim: as outras opções mantêm o índice.
    const juce::StringArray waveforms { "Sine", "Saw", "Square", "Triangle", "Custom" };

    for (size_t i = 0; i < ParamIDs::numOscillators; ++i)
    {
        const juce::String prefix = "Osc " + juce::String (i + 1) + " ";
        const bool isFirst = (i == 0);

        // Osc 1 ligado e osc 2/3 desligados: um set da v0.1 soa igual na v0.2.
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::oscOn[i], 1 }, prefix + "On", isFirst));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { ParamIDs::oscWave[i], 1 }, prefix + "Waveform", waveforms, 1));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::oscLevel[i], 1 }, prefix + "Level",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { ParamIDs::oscOctave[i], 1 }, prefix + "Octave", -3, 3, 0));

        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { ParamIDs::oscSteps[i], 1 }, prefix + "Steps", -24, 24, 0,
            juce::AudioParameterIntAttributes().withLabel ("steps")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::oscFine[i], 1 }, prefix + "Fine",
            juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ct")));

        // Unison (RF12). Com 1 cópia (padrão), o oscilador soa como antes.
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { ParamIDs::oscUnison[i], 1 }, prefix + "Unison", 1, Unison::maxVoices, 1,
            juce::AudioParameterIntAttributes().withLabel ("voices")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::oscDetune[i], 1 }, prefix + "Detune",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ct")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::oscBlend[i], 1 }, prefix + "Blend",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::oscWidth[i], 1 }, prefix + "Width",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));
    }

    // --- Vozes (RF7) ---
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParamIDs::voiceMode, 1 }, "Voice Mode",
        juce::StringArray { "Poly", "Mono" }, 0));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::legato, 1 }, "Legato", true));

    juce::NormalisableRange<float> glideRange (0.0f, 2000.0f, 0.1f);
    glideRange.setSkewForCentre (200.0f);
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::glide, 1 }, "Glide", glideRange, 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

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

    // --- Matriz de modulação (RF10) ---
    // Fonte e destino não são automatizáveis (trocar no meio de uma música geraria saltos);
    // a quantidade é. As listas vêm de ModMatrix e nunca mudam de ordem.
    juce::StringArray sourceNames, destNames;
    for (auto* name : ModMatrix::sourceNames) sourceNames.add (name);
    for (auto* name : ModMatrix::destNames)   destNames.add (name);

    for (size_t i = 0; i < ParamIDs::numModSlots; ++i)
    {
        const juce::String prefix = "Mod " + juce::String (i + 1) + " ";

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { ParamIDs::modSource[i], 1 }, prefix + "Source", sourceNames, 0,
            juce::AudioParameterChoiceAttributes().withAutomatable (false)));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { ParamIDs::modDest[i], 1 }, prefix + "Destination", destNames, 0,
            juce::AudioParameterChoiceAttributes().withAutomatable (false)));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::modAmount[i], 1 }, prefix + "Amount",
            juce::NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));
    }

    // --- LFOs (RF11) --- listas fechadas (ver Lfo.h)
    juce::StringArray shapeNames, divisionNames;
    for (auto* name : lfoShapeNames)    shapeNames.add (name);
    for (auto* name : lfoDivisionNames) divisionNames.add (name);

    juce::NormalisableRange<float> lfoRateRange (0.02f, 40.0f, 0.001f);
    lfoRateRange.setSkewForCentre (2.0f);

    for (size_t k = 0; k < ParamIDs::numLfos; ++k)
    {
        const juce::String prefix = "LFO " + juce::String (k + 1) + " ";

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { ParamIDs::lfoShape[k], 1 }, prefix + "Shape", shapeNames, 0));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParamIDs::lfoRate[k], 1 }, prefix + "Rate", lfoRateRange, 2.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::lfoSync[k], 1 }, prefix + "Sync", false));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { ParamIDs::lfoDivision[k], 1 }, prefix + "Division", divisionNames, lfoDefaultDivision));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { ParamIDs::lfoTrigger[k], 1 }, prefix + "Trigger",
            juce::StringArray { "Free", "Retrigger" }, 1));
    }

    // --- Envelope de modulação ---
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::modEnvAttack, 1 }, "Mod Env Attack", timeRange (5000.0f, 200.0f), 5.0f, msAttributes));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::modEnvDecay, 1 }, "Mod Env Decay", timeRange (5000.0f, 300.0f), 300.0f, msAttributes));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::modEnvSustain, 1 }, "Mod Env Sustain",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParamIDs::modEnvRelease, 1 }, "Mod Env Release", timeRange (10000.0f, 500.0f), 300.0f, msAttributes));

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
    for (size_t i = 0; i < oscParams.size(); ++i)
    {
        auto& osc = params.oscillators[i];
        osc.enabled   = oscParams[i].on->load() > 0.5f;
        osc.waveform  = static_cast<Waveform> (static_cast<int> (oscParams[i].wave->load()));
        osc.level     = oscParams[i].level->load() / 100.0f; // % -> 0..1
        osc.octave    = static_cast<int> (oscParams[i].octave->load());
        osc.steps     = static_cast<int> (oscParams[i].steps->load());
        osc.fineCents = oscParams[i].fine->load();
        osc.unison      = static_cast<int> (oscParams[i].unison->load());
        osc.detuneCents = oscParams[i].detune->load();
        osc.blend       = oscParams[i].blend->load() / 100.0f; // % -> 0..1
        osc.width       = oscParams[i].width->load() / 100.0f;
    }

    params.glideSeconds = glideParam->load() / 1000.0f; // ms -> s

    params.modEnv.attack  = modEnvAttackParam->load() / 1000.0f;
    params.modEnv.decay   = modEnvDecayParam->load() / 1000.0f;
    params.modEnv.sustain = modEnvSustainParam->load();
    params.modEnv.release = modEnvReleaseParam->load() / 1000.0f;

    for (size_t k = 0; k < lfoParams.size(); ++k)
    {
        auto& lfo = params.lfos[k];
        lfo.shape     = static_cast<LfoShape> (static_cast<int> (lfoParams[k].shape->load()));
        lfo.rateHz    = lfoRatesHz[k]; // calculado em updateLfoClock
        lfo.retrigger = lfoParams[k].trigger->load() > 0.5f;
    }

    // Formas desenhadas (RF15): a tabela publicada mais recente de cada uma. Sem lock nem
    // alocação; os ponteiros valem até o próximo bloco.
    for (size_t k = 0; k < params.lfoCustomTables.size(); ++k)
        params.lfoCustomTables[k] = customShapes->readLfoTable (k);
    for (size_t i = 0; i < params.oscCustomTables.size(); ++i)
        params.oscCustomTables[i] = customShapes->readOscTables (i);

    for (size_t i = 0; i < modSlotParams.size(); ++i)
    {
        auto& slot = params.modSlots[i];
        slot.source = static_cast<ModMatrix::Source> (static_cast<int> (modSlotParams[i].source->load()));
        slot.dest   = static_cast<ModMatrix::Dest>   (static_cast<int> (modSlotParams[i].dest->load()));
        slot.amount = modSlotParams[i].amount->load() / 100.0f; // % -> -1..+1
    }
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

    // Tempo do host e posição dos LFOs no modo Free para este bloco (RF11).
    updateLfoClock();

    // Osciladores, envelope e filtro valem em tempo real, inclusive para notas presas.
    updateVoices();

    // RF7: Poly ou Mono (trocar de modo solta as notas que estão tocando).
    synth.setVoiceMode (voiceModeParam->load() > 0.5f, legatoParam->load() > 0.5f);

    const int numSamples = buffer.getNumSamples();
    synth.renderNextBlock (buffer, midi, 0, numSamples);
    publishEnvelopeDisplay();

    // O relógio dos LFOs no modo Free anda o tamanho do bloco.
    for (size_t k = 0; k < freeLfos.size(); ++k)
        freeLfos[k].advance (getSampleRate() > 0.0 ? lfoRatesHz[k] * numSamples / getSampleRate() : 0.0);

    // Volume de saída, suavizado amostra a amostra para não estalar.
    gainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (gainParam->load()));

    for (int s = 0; s < numSamples; ++s)
    {
        const float g = gainSmoothed.getNextValue();

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.getWritePointer (ch)[s] *= g;
    }
}

void PluginProcessor::updateLfoClock()
{
    // Tempo e posição da música vêm do host (o Ableton). getPosition não aloca: pode ser
    // chamado na thread de áudio. juce::Optional ~ Optional do Java: pode não ter valor.
    double bpm = 120.0;
    juce::Optional<double> ppq; // posição em semínimas, só com a música tocando

    if (auto* hostPlayHead = getPlayHead())
    {
        if (auto position = hostPlayHead->getPosition())
        {
            if (auto hostBpm = position->getBpm())
                bpm = *hostBpm;

            if (position->getIsPlaying())
                ppq = position->getPpqPosition();
        }
    }

    for (size_t k = 0; k < lfoParams.size(); ++k)
    {
        const bool sync = lfoParams[k].sync->load() > 0.5f;
        const int division = juce::jlimit (0, static_cast<int> (lfoDivisionBeats.size()) - 1,
                                           static_cast<int> (lfoParams[k].division->load()));
        const double beats = lfoDivisionBeats[static_cast<size_t> (division)];

        lfoRatesHz[k] = sync ? Lfo::syncedHz (bpm, beats) : static_cast<double> (lfoParams[k].rate->load());

        // Free + Sync com a música tocando: a fase segue a posição na música, então o LFO
        // cai sempre no mesmo ponto do compasso.
        if (sync && ppq.hasValue())
            freeLfos[k].setPosition (*ppq / beats, 0);

        globalMod.lfoPositions[k] = { freeLfos[k].getPhase(), freeLfos[k].getCycle() };
    }
}

void PluginProcessor::publishEnvelopeDisplay()
{
    // A voz ativa que começou por último é a "última nota tocada".
    MicroVoice* newest = nullptr;

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* voice = dynamic_cast<MicroVoice*> (synth.getVoice (i)))
            if (voice->isVoiceActive() && (newest == nullptr || newest->wasStartedBefore (*voice)))
                newest = voice;

    MicroVoice::EnvelopeSnapshot snapshot; // idle se nenhuma voz estiver tocando
    if (newest != nullptr)
        snapshot = newest->getEnvelopeSnapshot();

    // Ponto do editor do LFO (RF15): no modo Free, a fase comum; no Retrigger, a da última nota.
    for (size_t k = 0; k < lfoDisplayPhase.size(); ++k)
    {
        const bool retrigger = lfoParams[k].trigger->load() > 0.5f;
        float phase = -1.0f;
        if (! retrigger)
            phase = static_cast<float> (freeLfos[k].getPhase());
        else if (newest != nullptr)
            phase = static_cast<float> (newest->getLfoPhase (k));
        lfoDisplayPhase[k].store (phase);
    }

    envelopeDisplay.stage.store (static_cast<int> (snapshot.stage));
    envelopeDisplay.secondsInStage.store (static_cast<float> (snapshot.secondsInStage));
    envelopeDisplay.level.store (snapshot.level);

    modEnvDisplay.stage.store (static_cast<int> (snapshot.stage));
    modEnvDisplay.secondsInStage.store (static_cast<float> (snapshot.secondsInStage));
    modEnvDisplay.level.store (snapshot.modLevel);
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    // Interface própria (etapa 6). Para depurar, juce::GenericAudioProcessorEditor (*this)
    // ainda mostra um slider por parâmetro.
    return new PluginEditor (*this);
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            loadShapesFromState();
        }
    }
}

void PluginProcessor::setLfoPoints (std::size_t lfo, const ShapePoints& points)
{
    customShapes->setLfoPoints (lfo, points);
    // Guarda no estado: vai junto com o set quando o Ableton salvar.
    apvts.state.setProperty (lfoPointsId (lfo), juce::String (points.toString()), nullptr);
}

void PluginProcessor::setOscPoints (std::size_t osc, const ShapePoints& points)
{
    customShapes->setOscPoints (osc, points);
    apvts.state.setProperty (oscPointsId (osc), juce::String (points.toString()), nullptr);
}

void PluginProcessor::loadShapesFromState()
{
    // Set salvo antes do RF15 não tem as propriedades: os pontos iniciais continuam.
    for (std::size_t k = 0; k < ParamIDs::numLfos; ++k)
        if (auto points = ShapePoints::fromString (apvts.state.getProperty (lfoPointsId (k)).toString().toStdString()))
            customShapes->setLfoPoints (k, *points);

    for (std::size_t i = 0; i < ParamIDs::numOscillators; ++i)
        if (auto points = ShapePoints::fromString (apvts.state.getProperty (oscPointsId (i)).toString().toStdString()))
            customShapes->setOscPoints (i, *points);
}

// Ponto de entrada que o host chama para criar o plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
