#include "Oscillator.h"

#include <cmath>
#include <numbers>

void Oscillator::setFrequency (double hz, double sampleRate) noexcept
{
    increment = sampleRate > 0.0 ? hz / sampleRate : 0.0;
}

void Oscillator::setPhase (double newPhase) noexcept
{
    phase = newPhase - std::floor (newPhase);
}

double Oscillator::polyBlep (double t, double dt) noexcept
{
    if (dt <= 0.0)
        return 0.0;

    if (t < dt) // logo depois do degrau
    {
        t /= dt;
        return t + t - t * t - 1.0;
    }

    if (t > 1.0 - dt) // logo antes do degrau
    {
        t = (t - 1.0) / dt;
        return t * t + t + t + 1.0;
    }

    return 0.0; // longe do degrau: sem correção
}

float Oscillator::nextSample() noexcept
{
    const double t  = phase;
    const double dt = increment;
    double value = 0.0;

    switch (waveform)
    {
        case Waveform::sine:
            value = std::sin (2.0 * std::numbers::pi * t);
            break;

        case Waveform::saw:
            // Rampa de -1 a +1 com degrau para baixo em t = 0.
            value = 2.0 * t - 1.0;
            value -= polyBlep (t, dt);
            break;

        case Waveform::square:
        {
            // +1 na primeira metade, -1 na segunda: degraus em t = 0 (sobe) e t = 0,5 (desce).
            value = t < 0.5 ? 1.0 : -1.0;
            value += polyBlep (t, dt);
            value -= polyBlep (std::fmod (t + 0.5, 1.0), dt);
            break;
        }

        case Waveform::triangle:
        {
            // Sem degraus (só "quinas"): os harmônicos caem rápido (1/n²) e o aliasing é baixo,
            // então dispensa PolyBLEP. Deslocada 1/4 de ciclo para começar em 0, como a senoide.
            const double shifted = std::fmod (t + 0.25, 1.0);
            value = 4.0 * std::abs (shifted - 0.5) - 1.0;
            break;
        }
    }

    phase += increment;
    phase -= std::floor (phase); // volta para [0, 1), mesmo se o incremento for > 1

    return static_cast<float> (value);
}
