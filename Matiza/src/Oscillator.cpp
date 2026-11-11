#include "Oscillator.h"

#include <algorithm>
#include <cmath>

void Oscillator::setWaveform (Waveform newWaveform) noexcept
{
    if (newWaveform == waveform)
        return;

    waveform = newWaveform;
    updateTable();
}

void Oscillator::setFrequency (double hz, double sampleRate) noexcept
{
    increment = sampleRate > 0.0 ? hz / sampleRate : 0.0;
    updateTable();
}

void Oscillator::setPhase (double newPhase) noexcept
{
    phase = newPhase - std::floor (newPhase);
}

void Oscillator::updateTable() noexcept
{
    table = Wavetables::table (waveform, Wavetables::levelFor (increment));
}
