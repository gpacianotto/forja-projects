#include "MicroVoice.h"
#include "MicroSound.h"

#include <utility>

void MicroVoice::prepare (double sampleRate, int maxBlockSize)
{
    adsr.setSampleRate (sampleRate);

    // Uma voz gera um sinal mono; ele é copiado para os canais no renderNextBlock.
    filter.prepare ({ sampleRate, static_cast<juce::uint32> (maxBlockSize), 1 });
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    cutoffSmoothed.reset (sampleRate, 0.02);
    cutoffSmoothed.setCurrentAndTargetValue (clampCutoff (8000.0f));
    filter.setCutoffFrequency (cutoffSmoothed.getCurrentValue());

    for (auto& oscLevel : oscLevels)
    {
        oscLevel.reset (sampleRate, 0.02);
        oscLevel.setCurrentAndTargetValue (0.0f);
    }
}

float MicroVoice::clampCutoff (float hz) const
{
    // O filtro exige cutoff abaixo de Nyquist (metade da taxa de amostragem).
    const auto maxHz = static_cast<float> (getSampleRate() * 0.45);
    return juce::jlimit (20.0f, juce::jmax (20.0f, maxHz), hz);
}

void MicroVoice::setParameters (const VoiceParameters& params)
{
    parameters = params;

    for (size_t i = 0; i < oscillators.size(); ++i)
        oscillators[i].setWaveform (params.oscillators[i].waveform);

    // Só repassa ao ADSR quando algo mudou: juce::ADSR::setParameters recalcula a velocidade
    // do release a partir do sustain, o que, chamado a cada bloco durante um release,
    // deformava a curva (e com sustain = 0 cortava a nota de uma vez, com estalo).
    const auto& a = params.adsr;
    const auto& current = adsr.getParameters();
    if (a.attack != current.attack || a.decay != current.decay
        || a.sustain != current.sustain || a.release != current.release)
        adsr.setParameters (a);
    filter.setResonance (params.resonance);
    cutoffSmoothed.setTargetValue (clampCutoff (params.cutoffHz));
    glide.setTime (params.glideSeconds, getSampleRate());

    // Oitava, passos e cents valem em tempo real, inclusive para notas presas.
    if (isVoiceActive())
        updateFrequencies();
}

bool MicroVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<MicroSound*> (sound) != nullptr;
}

void MicroVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*,
                            int currentPitchWheelPosition)
{
    // std::exchange: lê o valor e já zera a flag (vale só para esta nota).
    const bool handoff = std::exchange (monoHandoffPending, false);

    noteTuning = tuning; // cópia (sem alocação: só números)
    bend       = TuningEngine::pitchWheelToBend (currentPitchWheelPosition);

    // Modo Mono com a voz ainda soando: troca de nota sem recomeçar do zero (RF7).
    if (handoff && adsr.isActive())
    {
        glide.glideTo (midiNoteNumber);
        updateFrequencies();

        if (monoRetrigger)
        {
            // juce::ADSR::noteOn parte do nível atual do envelope, então não estala.
            level = velocity;
            adsr.noteOn();
            samplesInStage = 0;
            released = false;
        }

        return;
    }

    // Nota nova numa voz parada.
    glide.jumpTo (midiNoteNumber);

    for (auto& oscillator : oscillators)
        oscillator.resetPhase();

    updateFrequencies();

    // Volumes dos osciladores já no valor certo, sem fade-in.
    for (auto& oscLevel : oscLevels)
        oscLevel.setCurrentAndTargetValue (oscLevel.getTargetValue());

    // Nota nova começa com o filtro "limpo" e já no cutoff atual, sem deslizar.
    filter.reset();
    cutoffSmoothed.setCurrentAndTargetValue (cutoffSmoothed.getTargetValue());
    filter.setCutoffFrequency (cutoffSmoothed.getCurrentValue());

    level = velocity;
    adsr.noteOn();
    samplesInStage = 0;
    released = false;
}

void MicroVoice::stopNote (float, bool allowTailOff)
{
    // Troca de nota no modo Mono: o juce::Synthesiser corta a nota antiga antes de começar
    // a nova; aqui ignoramos o corte para a voz seguir soando (o startNote faz a troca).
    if (monoHandoffPending)
        return;

    if (allowTailOff)
    {
        adsr.noteOff(); // entra no release; a voz é liberada em renderNextBlock quando acabar
        samplesInStage = 0;
        released = true;
    }
    else
    {
        // O host pediu silêncio imediato (ex.: roubo de voz ou "all notes off").
        adsr.reset();
        clearCurrentNote();
    }
}

MicroVoice::EnvelopeSnapshot MicroVoice::getEnvelopeSnapshot() const noexcept
{
    EnvelopeSnapshot snapshot;

    if (! isVoiceActive())
        return snapshot; // idle

    snapshot.stage = released ? GraphMath::EnvelopeStage::released : GraphMath::EnvelopeStage::held;
    snapshot.secondsInStage = getSampleRate() > 0.0 ? samplesInStage / getSampleRate() : 0.0;
    snapshot.level = lastEnvelopeLevel;
    return snapshot;
}

void MicroVoice::pitchWheelMoved (int newPitchWheelValue)
{
    bend = TuningEngine::pitchWheelToBend (newPitchWheelValue);
    updateFrequencies();
}

void MicroVoice::updateFrequencies()
{
    const double sampleRate = getSampleRate();

    // RF1: a frequência vem do motor de afinação (qualquer EDO), nunca do 12 tons do JUCE.
    const double noteHz = noteTuning.noteToHz (glide.getCurrent(), bend);
    const int edo = noteTuning.getSettings().edo;

    for (size_t i = 0; i < oscillators.size(); ++i)
    {
        const auto& osc = parameters.oscillators[i];
        const double hz = noteHz * TuningEngine::transposeRatio (edo, osc.octave, osc.steps, osc.fineCents);

        oscillators[i].setFrequency (hz, sampleRate);

        // RF6 + RF4: oscilador transposto para fora da faixa tocável fica mudo.
        const bool audible = osc.enabled && TuningEngine::isPlayable (hz, sampleRate);
        oscLevels[i].setTargetValue (audible ? osc.level : 0.0f);
    }
}

void MicroVoice::renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    const int numChannels = output.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        // Durante o glide a altura muda a cada amostra.
        if (glide.isGliding())
        {
            glide.next();
            updateFrequencies();
        }

        if (cutoffSmoothed.isSmoothing())
            filter.setCutoffFrequency (cutoffSmoothed.getNextValue());

        float raw = 0.0f;

        for (size_t o = 0; o < oscillators.size(); ++o)
        {
            const float oscLevel = oscLevels[o].getNextValue();

            // Oscilador mudo e parado em 0: não gasta CPU calculando a onda.
            if (oscLevel > 0.0f)
                raw += oscillators[o].nextSample() * oscLevel;
        }

        const float filtered = filter.processSample (0, raw);
        lastEnvelopeLevel    = adsr.getNextSample();
        const float sample   = filtered * lastEnvelopeLevel * level;
        ++samplesInStage;

        // Somar, não sobrescrever: as outras vozes escrevem no mesmo buffer.
        for (int ch = 0; ch < numChannels; ++ch)
            output.addSample (ch, startSample + i, sample);

        if (! adsr.isActive())
        {
            clearCurrentNote(); // fim do release: a voz fica livre para outra nota
            break;
        }
    }
}
