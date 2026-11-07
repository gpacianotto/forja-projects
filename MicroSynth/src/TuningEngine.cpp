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

double TuningEngine::pitchWheelToBend (int wheelValue) noexcept
{
    constexpr int center = 8192;
    const int offset = std::clamp (wheelValue, 0, 16383) - center;

    // A faixa é assimétrica (8192 valores abaixo do centro, 8191 acima); dividir cada lado
    // pelo seu tamanho garante que os extremos dêem exatamente -1 e +1.
    return offset < 0 ? offset / 8192.0 : offset / 8191.0;
}
