#pragma once

// Formas de onda do oscilador. A ordem é a mesma das opções do parâmetro "wave"
// (o índice do AudioParameterChoice vira este enum), então não reordenar.
enum class Waveform
{
    sine = 0,
    saw,
    square,
    triangle
};

inline constexpr int numWaveforms = 4;
