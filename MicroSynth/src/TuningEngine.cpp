#include "TuningEngine.h"

#include <algorithm>
#include <cmath>

void TuningEngine::setSettings (const Settings& newSettings) noexcept
{
    settings = newSettings;
    settings.edo = std::max (1, settings.edo); // evita divisão por zero
}

double TuningEngine::noteToHz (int midiNote, double bend) const noexcept
{
    // Quantos passos da EDO a nota está acima (ou abaixo) da referência, incluindo o bend.
    const double steps = (midiNote - settings.refNote) + bend * settings.bendSteps;

    // Cada passo multiplica a frequência por 2^(1/edo); edo passos = uma oitava = dobro.
    return settings.baseHz * std::exp2 (steps / settings.edo);
}
