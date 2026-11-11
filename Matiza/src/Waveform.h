#pragma once

// Formas de onda do oscilador. A ordem é a mesma das opções do parâmetro "wave"
// (o índice do AudioParameterChoice vira este enum), então não reordenar.
enum class Waveform
{
    sine = 0,
    saw,
    square,
    triangle,
    custom // forma desenhada (RF15); sempre a última
};

// Formas prontas, montadas pelo banco de tabelas (todas menos Custom).
inline constexpr int numBuiltinWaveforms = 4;
