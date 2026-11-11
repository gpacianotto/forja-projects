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

void Oscillator::setCustomTables (const float* levels) noexcept
{
    if (levels == customLevels)
        return;

    customLevels = levels;
    if (waveform == Waveform::custom)
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
    const int level = Wavetables::levelFor (increment);

    if (waveform == Waveform::custom && customLevels != nullptr)
        table = customLevels + level * Wavetables::levelStride;
    else
        table = Wavetables::table (waveform, level); // Custom sem tabela ainda: serra
}
