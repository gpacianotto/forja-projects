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

// Oscilador com acumulador de fase. C++ puro (sem JUCE), sem alocação: seguro na thread de áudio.
//
// Serra e quadrada têm "degraus" (saltos instantâneos) que, amostrados, geram aliasing:
// harmônicos acima de Nyquist que voltam espelhados como notas desafinadas. O PolyBLEP
// arredonda cada degrau com um pequeno polinômio nas duas amostras vizinhas, removendo
// boa parte desse aliasing a um custo baixíssimo.
class Oscillator
{
public:
    void setWaveform (Waveform newWaveform) noexcept { waveform = newWaveform; }
    void setFrequency (double hz, double sampleRate) noexcept;
    void resetPhase() noexcept { phase = 0.0; }

    // Devolve a amostra atual (entre -1 e 1, aprox.) e avança a fase.
    float nextSample() noexcept;

    // Correção PolyBLEP para um degrau em t = 0 (t = fase de 0 a 1, dt = incremento por amostra).
    // Público para poder ser testado.
    static double polyBlep (double t, double dt) noexcept;

private:
    Waveform waveform = Waveform::saw;
    double phase = 0.0;     // posição no ciclo, de 0 a 1
    double increment = 0.0; // frequência / sample rate
};
