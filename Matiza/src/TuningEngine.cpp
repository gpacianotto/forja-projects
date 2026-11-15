#include "TuningEngine.h"

#include <algorithm>
#include <cmath>

void TuningEngine::setSettings (const Settings& newSettings) noexcept
{
    settings = newSettings;
    settings.edo = std::max (1, settings.edo); // evita divisão por zero
}

namespace
{
    // Cents absolutos (a partir de 1 Hz) de uma frequência: 1200 × log2(Hz).
    double absoluteCents (double hz) noexcept
    {
        return 1200.0 * std::log2 (hz);
    }

    // 12-TET com A = 440 Hz, para quando o master entrega uma frequência inválida.
    double standardHz (int midiNote) noexcept
    {
        return 440.0 * std::exp2 ((midiNote - 69) / 12.0);
    }
}

void TuningEngine::setMasterTuning (const MasterTuning& master) noexcept
{
    masterActive = master.active;
    if (! masterActive)
        return; // a tabela antiga fica, mas não vale

    for (std::size_t n = 0; n < masterCents.size(); ++n)
    {
        const double hz = master.hz[n];
        const bool valid = std::isfinite (hz) && hz > 0.0;
        masterCents[n]    = absoluteCents (valid ? hz : standardHz (static_cast<int> (n)));
        masterFiltered[n] = master.filtered[n];
    }
}

bool TuningEngine::isNoteFiltered (int midiNote) const noexcept
{
    if (! masterActive || midiNote < 0 || midiNote >= numMidiNotes)
        return false;

    return masterFiltered[static_cast<std::size_t> (midiNote)];
}

double TuningEngine::masterNoteCents (int note) const noexcept
{
    // Fora de 0..127 a tabela continua com o intervalo da ponta (RF18): 0 → 1 para baixo,
    // 126 → 127 para cima.
    constexpr int last = numMidiNotes - 1;
    if (note < 0)
        return masterCents[0] + note * (masterCents[1] - masterCents[0]);
    if (note > last)
        return masterCents[last] + (note - last) * (masterCents[last] - masterCents[last - 1]);

    return masterCents[static_cast<std::size_t> (note)];
}

double TuningEngine::masterAbsoluteCents (double note) const noexcept
{
    // Notas inteiras caem exatamente na tabela; entre duas notas (glide, bend), linha reta em
    // cents, como na afinação desenhada (RF17).
    const double lower = std::floor (note);
    const double frac  = note - lower;
    const int    i     = static_cast<int> (lower);

    const double a = masterNoteCents (i);
    return frac == 0.0 ? a : a + (masterNoteCents (i + 1) - a) * frac;
}

double TuningEngine::noteToHz (double midiNote, double bend, double extraSteps) const noexcept
{
    // RF18: com o master, "step" = nota MIDI; a altura vem direto da tabela dele.
    if (masterActive)
        return std::exp2 (masterAbsoluteCents (midiNote + bend * settings.bendSteps + extraSteps) / 1200.0);

    // Cada 1200 cents = uma oitava = dobro da frequência.
    return settings.baseHz * std::exp2 (centsFromBase (midiNote, bend, extraSteps) / 1200.0);
}

double TuningEngine::centsFromBase (double midiNote, double bend, double extraSteps) const noexcept
{
    if (masterActive)
        return masterAbsoluteCents (midiNote + bend * settings.bendSteps + extraSteps)
             - absoluteCents (settings.baseHz);

    // Quantos passos da EDO a nota está acima (ou abaixo) da referência, incluindo o bend e a
    // transposição. Na afinação igual, cada passo vale 1200/edo cents; com tabela, cada passo
    // cai na linha desenhada (TuningTable::stepsToCents).
    const double steps = (midiNote - settings.refNote) + bend * settings.bendSteps + extraSteps;
    return settings.table.stepsToCents (steps, settings.edo);
}

double TuningEngine::transposeRatio (int edo, int octaves, double steps, double cents) noexcept
{
    return std::exp2 (octaves + steps / std::max (1, edo) + cents / 1200.0);
}

double TuningEngine::pitchWheelToBend (int wheelValue) noexcept
{
    constexpr int center = 8192;
    const int offset = std::clamp (wheelValue, 0, 16383) - center;

    // A faixa é assimétrica (8192 valores abaixo do centro, 8191 acima); dividir cada lado
    // pelo seu tamanho garante que os extremos dêem exatamente -1 e +1.
    return offset < 0 ? offset / 8192.0 : offset / 8191.0;
}

bool TuningEngine::isPlayable (double hz, double sampleRate) noexcept
{
    return sampleRate > 0.0
        && hz >= minAudibleHz
        && hz <= maxNyquistFraction * sampleRate;
}
