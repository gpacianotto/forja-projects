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

void Oscillator::setCustomTables (const float* levels, const float* residualLevels) noexcept
{
    if (levels == customLevels && residualLevels == customResidualLevels)
        return;

    customLevels = levels;
    customResidualLevels = residualLevels;
    if (waveform == Waveform::custom)
        updateTable();
}

void Oscillator::setResidual (bool useResidual) noexcept
{
    if (useResidual == residual)
        return;

    residual = useResidual;
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

    // Custom sem tabela ainda: serra.
    const float* custom = residual ? customResidualLevels : customLevels;

    if (waveform == Waveform::custom && custom != nullptr)
        table = custom + level * Wavetables::levelStride;
    else if (residual)
        table = Wavetables::residualTable (waveform, level);
    else
        table = Wavetables::table (waveform, level);
}
