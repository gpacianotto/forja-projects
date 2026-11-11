#include "MatizaVoice.h"
#include "MatizaSound.h"

#include <cmath>
#include <utility>

using ModMatrix::Dest;
using ModMatrix::Source;

void MatizaVoice::prepare (double sampleRate, int maxBlockSize)
{
    adsr.setSampleRate (sampleRate);
    modEnv.setSampleRate (sampleRate);

    // Voz estéreo (RF12): o unison espalha as cópias entre esquerda e direita, então o filtro
    // processa dois canais (cada um guarda seu próprio estado interno).
    filter.prepare ({ sampleRate, static_cast<juce::uint32> (maxBlockSize), 2 });
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);

    cutoffSmoothed.reset (sampleRate, 0.02);
    cutoffSmoothed.setCurrentAndTargetValue (clampCutoff (8000.0f));
    filter.setCutoffFrequency (cutoffSmoothed.getCurrentValue());

    for (auto& oscLevel : oscLevels)
    {
        oscLevel.reset (sampleRate, 0.02);
        oscLevel.setCurrentAndTargetValue (0.0f);
    }

    modWheelSmoothed.reset (sampleRate, 0.02);
    aftertouchSmoothed.reset (sampleRate, 0.02);

    for (size_t o = 0; o < blendSmoothed.size(); ++o)
    {
        blendSmoothed[o].reset (sampleRate, 0.02);
        widthSmoothed[o].reset (sampleRate, 0.02);
        blendSmoothed[o].setCurrentAndTargetValue (parameters.oscillators[o].blend);
        widthSmoothed[o].setCurrentAndTargetValue (parameters.oscillators[o].width);
    }

    // Interpolação da modulação: anda até o novo valor em exatamente um intervalo de recálculo.
    cutoffModRatio.reset (modUpdateInterval);
    cutoffModRatio.setCurrentAndTargetValue (1.0f);

    for (auto& levelMod : oscLevelMods)
    {
        levelMod.reset (modUpdateInterval);
        levelMod.setCurrentAndTargetValue (0.0f);
    }
}

float MatizaVoice::clampCutoff (float hz) const
{
    // O filtro exige cutoff abaixo de Nyquist (metade da taxa de amostragem).
    const auto maxHz = static_cast<float> (getSampleRate() * 0.45);
    return juce::jlimit (20.0f, juce::jmax (20.0f, maxHz), hz);
}

void MatizaVoice::setEnvelopeParameters (juce::ADSR& envelope, const juce::ADSR::Parameters& p)
{
    // Só repassa ao ADSR quando algo mudou: juce::ADSR::setParameters recalcula a velocidade
    // do release a partir do sustain, o que, chamado a cada bloco durante um release,
    // deformava a curva (e com sustain = 0 cortava a nota de uma vez, com estalo).
    const auto& current = envelope.getParameters();
    if (p.attack != current.attack || p.decay != current.decay
        || p.sustain != current.sustain || p.release != current.release)
        envelope.setParameters (p);
}

void MatizaVoice::setParameters (const VoiceParameters& params)
{
    parameters = params;

    for (size_t o = 0; o < oscillators.size(); ++o)
    {
        for (auto& copy : oscillators[o])
        {
            copy.setCustomTables (params.oscCustomTables[o]);
            copy.setWaveform (params.oscillators[o].waveform);
        }

        blendSmoothed[o].setTargetValue (params.oscillators[o].blend);
        widthSmoothed[o].setTargetValue (params.oscillators[o].width);
    }

    updateUnisonLayouts();

    setEnvelopeParameters (adsr, params.adsr);
    setEnvelopeParameters (modEnv, params.modEnv);

    for (size_t k = 0; k < lfos.size(); ++k)
    {
        lfos[k].setShape (params.lfos[k].shape);
        lfos[k].setCustomTable (params.lfoCustomTables[k]);
    }
    glide.setTime (params.glideSeconds, getSampleRate());

    // Knobs (e a matriz em cima deles) valem em tempo real, inclusive para notas presas.
    updateModulation();
}

bool MatizaVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<MatizaSound*> (sound) != nullptr;
}

void MatizaVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*,
                            int currentPitchWheelPosition)
{
    // std::exchange: lê o valor e já zera a flag (vale só para esta nota).
    const bool handoff = std::exchange (monoHandoffPending, false);

    noteTuning      = tuning; // cópia (sem alocação: só números)
    bend            = TuningEngine::pitchWheelToBend (currentPitchWheelPosition);
    currentMidiNote = midiNoteNumber;

    // Modo Mono com a voz ainda soando: troca de nota sem recomeçar do zero (RF7).
    if (handoff && adsr.isActive())
    {
        glide.glideTo (midiNoteNumber);

        if (monoRetrigger)
        {
            // juce::ADSR::noteOn parte do nível atual do envelope, então não estala.
            level = velocity;
            adsr.noteOn();
            modEnv.noteOn();
            restartLfos();
            samplesInStage = 0;
            released = false;
        }

        updateModulation();
        return;
    }

    // Nota nova numa voz parada.
    glide.jumpTo (midiNoteNumber);
    level = velocity;
    polyAftertouch = 0.0f;

    // Fontes globais já no valor atual, sem deslizar a partir do valor da nota anterior.
    modWheelSmoothed.setCurrentAndTargetValue (globalMod.modWheel);
    aftertouchSmoothed.setCurrentAndTargetValue (globalMod.channelPressure);

    // Fase inicial (RF12): com uma cópia, começa do zero, como antes do unison. Com várias,
    // cada uma começa num ponto aleatório do ciclo; se todas começassem juntas, o ataque soaria
    // como um "flanger" até as cópias se separarem.
    for (size_t o = 0; o < oscillators.size(); ++o)
    {
        const bool spread = parameters.oscillators[o].unison > 1;

        for (auto& copy : oscillators[o])
        {
            if (spread)
                copy.setPhase (random.nextDouble());
            else
                copy.resetPhase();
        }

        blendSmoothed[o].setCurrentAndTargetValue (parameters.oscillators[o].blend);
        widthSmoothed[o].setCurrentAndTargetValue (parameters.oscillators[o].width);
    }

    updateUnisonLayouts();

    modEnv.reset();
    modEnvValue = 0.0f;
    restartLfos();
    updateModulation();

    // Volumes dos osciladores já no valor certo, sem fade-in.
    for (size_t o = 0; o < oscLevels.size(); ++o)
    {
        oscLevels[o].setCurrentAndTargetValue (oscLevels[o].getTargetValue());
        oscLevelMods[o].setCurrentAndTargetValue (oscLevelMods[o].getTargetValue());
    }

    // Nota nova começa com o filtro "limpo" e já no cutoff atual, sem deslizar.
    filter.reset();
    cutoffSmoothed.setCurrentAndTargetValue (cutoffSmoothed.getTargetValue());
    cutoffModRatio.setCurrentAndTargetValue (cutoffModRatio.getTargetValue());
    filter.setCutoffFrequency (clampCutoff (cutoffSmoothed.getCurrentValue() * cutoffModRatio.getCurrentValue()));

    adsr.noteOn();
    modEnv.noteOn();
    samplesInStage = 0;
    samplesUntilModUpdate = 0;
    released = false;
}

void MatizaVoice::stopNote (float, bool allowTailOff)
{
    // Troca de nota no modo Mono: o juce::Synthesiser corta a nota antiga antes de começar
    // a nova; aqui ignoramos o corte para a voz seguir soando (o startNote faz a troca).
    if (monoHandoffPending)
        return;

    if (allowTailOff)
    {
        adsr.noteOff(); // entra no release; a voz é liberada em renderNextBlock quando acabar
        modEnv.noteOff();
        samplesInStage = 0;
        released = true;
    }
    else
    {
        // O host pediu silêncio imediato (ex.: roubo de voz ou "all notes off").
        adsr.reset();
        modEnv.reset();
        clearCurrentNote();
    }
}

void MatizaVoice::restartLfos()
{
    // Modo Retrigger: cada nota começa o LFO do zero e sorteia uma semente nova para o
    // Sample & Hold. (No modo Free a posição é sobrescrita pela global no próximo bloco.)
    for (size_t k = 0; k < lfos.size(); ++k)
    {
        lfos[k].reset();
        noteSeeds[k] = static_cast<std::uint64_t> (random.nextInt64());
    }
}

void MatizaVoice::aftertouchChanged (int newAftertouchValue)
{
    polyAftertouch = static_cast<float> (newAftertouchValue) / 127.0f;
}

MatizaVoice::EnvelopeSnapshot MatizaVoice::getEnvelopeSnapshot() const noexcept
{
    EnvelopeSnapshot snapshot;

    if (! isVoiceActive())
        return snapshot; // idle

    snapshot.stage = released ? GraphMath::EnvelopeStage::released : GraphMath::EnvelopeStage::held;
    snapshot.secondsInStage = getSampleRate() > 0.0 ? samplesInStage / getSampleRate() : 0.0;
    snapshot.level    = lastEnvelopeLevel;
    snapshot.modLevel = modEnvValue;
    return snapshot;
}

void MatizaVoice::pitchWheelMoved (int newPitchWheelValue)
{
    bend = TuningEngine::pitchWheelToBend (newPitchWheelValue);
    updateFrequencies();
}

void MatizaVoice::updateModulation()
{
    // 1. Valor atual de cada fonte.
    ModMatrix::SourceValues sources {};
    sources[ModMatrix::index (Source::velocity)]   = level;
    sources[ModMatrix::index (Source::modWheel)]   = modWheelSmoothed.getCurrentValue();
    sources[ModMatrix::index (Source::aftertouch)] = juce::jmax (aftertouchSmoothed.getCurrentValue(), polyAftertouch);
    sources[ModMatrix::index (Source::keyTrack)]   = ModMatrix::keyTrack (currentMidiNote);
    sources[ModMatrix::index (Source::modEnv)]     = modEnvValue;
    sources[ModMatrix::index (Source::lfo1)]       = lfos[0].value();
    sources[ModMatrix::index (Source::lfo2)]       = lfos[1].value();
    sources[ModMatrix::index (Source::lfo3)]       = lfos[2].value();

    // 2. Deslocamento de cada destino.
    ModMatrix::computeOffsets (parameters.modSlots, sources, modOffsets);

    // 3. Filtro: soma na posição normalizada do knob e converte de volta para Hz / Q.
    auto modulate = [this] (const juce::NormalisableRange<float>& range, float base, Dest dest)
    {
        const float offset = modOffsets[ModMatrix::index (dest)];
        if (offset == 0.0f)
            return base; // sem modulação: valor exato do knob

        return range.convertFrom0to1 (ModMatrix::applyNormalized (range.convertTo0to1 (base), offset));
    };

    // Cutoff = valor do knob (suavizado em 20 ms) × razão da modulação (interpolada em 32 amostras).
    const float baseCutoff = clampCutoff (parameters.cutoffHz);
    const float modulatedCutoff = clampCutoff (modulate (ranges.cutoff, parameters.cutoffHz, Dest::cutoff));
    cutoffSmoothed.setTargetValue (baseCutoff);
    cutoffModRatio.setTargetValue (modulatedCutoff / baseCutoff);
    filter.setResonance (modulate (ranges.resonance, parameters.resonance, Dest::resonance));

    // 4. Osciladores (volume, altura e fine).
    if (isVoiceActive())
        updateFrequencies();
}

void MatizaVoice::updateUnisonLayouts()
{
    // Desvio, volume e pan de cada cópia (RF12). Blend e Width vêm suavizados.
    for (size_t o = 0; o < unisonLayouts.size(); ++o)
    {
        const auto& osc = parameters.oscillators[o];
        unisonLayouts[o] = Unison::computeLayout (osc.unison, osc.detuneCents,
                                                  blendSmoothed[o].getCurrentValue(),
                                                  widthSmoothed[o].getCurrentValue());
    }
}

void MatizaVoice::updateFrequencies()
{
    const double sampleRate = getSampleRate();

    // RF1: a frequência vem do motor de afinação (qualquer EDO), nunca do 12 tons do JUCE.
    const double noteHz = noteTuning.noteToHz (glide.getCurrent(), bend);
    const int edo = noteTuning.getSettings().edo;

    const double globalPitchSteps = modOffsets[ModMatrix::index (Dest::pitch)] * ModMatrix::pitchStepsAtFull;

    for (size_t i = 0; i < oscillators.size(); ++i)
    {
        const auto& osc = parameters.oscillators[i];

        // Matriz: Pitch em passos da EDO; Level e Fine na faixa normalizada do knob.
        const double steps = osc.steps + globalPitchSteps
                           + modOffsets[ModMatrix::index (ModMatrix::oscPitchDest (i))] * ModMatrix::pitchStepsAtFull;

        const float fineNormalized = (osc.fineCents + 100.0f) / 200.0f; // -100..+100 ct -> 0..1
        const float fineCents = ModMatrix::applyNormalized (fineNormalized, modOffsets[ModMatrix::index (ModMatrix::oscFineDest (i))]) * 200.0f - 100.0f;

        const float oscLevel = ModMatrix::applyNormalized (osc.level, modOffsets[ModMatrix::index (ModMatrix::oscLevelDest (i))]);

        const double hz = noteHz * TuningEngine::transposeRatio (edo, osc.octave, steps, fineCents);

        // Unison (RF12): cada cópia desvia alguns cents da frequência do oscilador.
        // Cópia que passaria de Nyquist (RF4) fica muda; as outras continuam.
        const auto& unisonLayout = unisonLayouts[i];
        bool anyCopyPlayable = false;

        for (int k = 0; k < unisonLayout.count; ++k)
        {
            const double copyHz = hz * std::pow (2.0, unisonLayout.detuneCents[k] / 1200.0);
            const bool playable = TuningEngine::isPlayable (copyHz, sampleRate);

            oscillators[i][k].setFrequency (copyHz, sampleRate);
            copyAudible[i][k] = playable;
            anyCopyPlayable = anyCopyPlayable || playable;
        }

        // RF6 + RF4: oscilador transposto para fora da faixa tocável fica mudo.
        // Volume = valor do knob (suavizado em 20 ms) + parte da matriz (interpolada em 32 amostras).
        const bool audible = osc.enabled && anyCopyPlayable;
        oscLevels[i].setTargetValue (audible ? osc.level : 0.0f);
        oscLevelMods[i].setTargetValue (audible ? oscLevel - osc.level : 0.0f);
    }
}

void MatizaVoice::renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    const int numChannels = output.getNumChannels();

    modWheelSmoothed.setTargetValue (globalMod.modWheel);
    aftertouchSmoothed.setTargetValue (globalMod.channelPressure);

    // LFOs: quanto cada um anda por amostra e, no modo Free, a posição global do bloco
    // (mais as amostras antes de startSample, porque o bloco pode chegar em pedaços).
    const double sampleRate = getSampleRate();
    std::array<double, ParamIDs::numLfos> lfoIncrements {};

    for (size_t k = 0; k < lfos.size(); ++k)
    {
        const auto& lfoParams = parameters.lfos[k];
        lfoIncrements[k] = sampleRate > 0.0 ? lfoParams.rateHz / sampleRate : 0.0;

        if (lfoParams.retrigger)
        {
            lfos[k].setSeed (noteSeeds[k]);
        }
        else
        {
            const auto& position = globalMod.lfoPositions[k];
            lfos[k].setPosition (position.phase, position.cycle);
            lfos[k].advance (startSample * lfoIncrements[k]);
            lfos[k].setSeed (k + 1); // mesma semente em todas as vozes: mesmo Sample & Hold
        }
    }

    for (int i = 0; i < numSamples; ++i)
    {
        modWheelSmoothed.getNextValue();
        aftertouchSmoothed.getNextValue();
        modEnvValue = modEnv.getNextSample();

        // Matriz recalculada a cada 32 amostras (~0,7 ms a 48 kHz): rápido o bastante para
        // soar contínuo e barato o bastante para 8 vozes.
        // Durante o glide a altura muda a cada amostra.
        const bool gliding = glide.isGliding();
        if (gliding)
            glide.next();

        if (--samplesUntilModUpdate <= 0)
        {
            // Blend e Width andam um intervalo inteiro de uma vez; o volume e o pan das cópias
            // mudam em passos de 32 amostras (~0,7 ms), pequenos demais para estalar.
            for (size_t o = 0; o < blendSmoothed.size(); ++o)
            {
                blendSmoothed[o].skip (modUpdateInterval);
                widthSmoothed[o].skip (modUpdateInterval);
            }

            updateUnisonLayouts();
            updateModulation(); // inclui updateFrequencies
            samplesUntilModUpdate = modUpdateInterval;
        }
        else if (gliding)
        {
            updateFrequencies();
        }

        if (cutoffSmoothed.isSmoothing() || cutoffModRatio.isSmoothing())
            filter.setCutoffFrequency (clampCutoff (cutoffSmoothed.getNextValue() * cutoffModRatio.getNextValue()));

        for (size_t k = 0; k < lfos.size(); ++k)
            lfos[k].advance (lfoIncrements[k]);

        float rawLeft = 0.0f, rawRight = 0.0f;

        for (size_t o = 0; o < oscillators.size(); ++o)
        {
            const float oscLevel = juce::jmax (0.0f, oscLevels[o].getNextValue() + oscLevelMods[o].getNextValue());

            // Oscilador mudo e parado em 0: não gasta CPU calculando a onda.
            if (oscLevel <= 0.0f)
                continue;

            const auto& unisonLayout = unisonLayouts[o];
            float left = 0.0f, right = 0.0f;

            for (int k = 0; k < unisonLayout.count; ++k)
            {
                if (! copyAudible[o][k])
                    continue;

                const float copySample = oscillators[o][k].nextSample();
                left  += copySample * unisonLayout.gainLeft[k];
                right += copySample * unisonLayout.gainRight[k];
            }

            rawLeft  += left * oscLevel;
            rawRight += right * oscLevel;
        }

        const float filteredLeft  = filter.processSample (0, rawLeft);
        const float filteredRight = filter.processSample (1, rawRight);
        lastEnvelopeLevel = adsr.getNextSample();
        const float gain  = lastEnvelopeLevel * level;
        ++samplesInStage;

        // Somar, não sobrescrever: as outras vozes escrevem no mesmo buffer.
        if (numChannels == 1)
        {
            output.addSample (0, startSample + i, 0.5f * (filteredLeft + filteredRight) * gain);
        }
        else
        {
            output.addSample (0, startSample + i, filteredLeft * gain);
            output.addSample (1, startSample + i, filteredRight * gain);
        }

        if (! adsr.isActive())
        {
            clearCurrentNote(); // fim do release: a voz fica livre para outra nota
            break;
        }
    }
}
