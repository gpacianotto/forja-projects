#include "TuningEngine.h"

#include <algorithm>
#include <cmath>

void TuningEngine::setSettings (const Settings& newSettings) noexcept
{
    settings = newSettings;
    settings.edo = std::max (1, settings.edo); // evita divisão por zero
}

double TuningEngine::noteToHz (double midiNote, double bend, double extraSteps) const noexcept
{
    // Cada 1200 cents = uma oitava = dobro da frequência.
    return settings.baseHz * std::exp2 (centsFromBase (midiNote, bend, extraSteps) / 1200.0);
}

double TuningEngine::centsFromBase (double midiNote, double bend, double extraSteps) const noexcept
{
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
