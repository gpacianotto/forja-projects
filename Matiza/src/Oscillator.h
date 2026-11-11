#pragma once

#include <algorithm>
#include <cmath>

#include "Waveform.h"
#include "Wavetables.h"

// Oscilador de wavetable (RF13). C++ puro (sem JUCE), sem alocação: seguro na thread de áudio.
//
// Lê um ciclo da onda guardado numa tabela (Wavetables.h), andando pela tabela com um
// acumulador de fase. A tabela usada depende da frequência: notas agudas leem versões com
// menos harmônicos (mipmaps), para nenhum harmônico passar de Nyquist e virar aliasing.
class Oscillator
{
public:
    void setWaveform (Waveform newWaveform) noexcept;
    void setFrequency (double hz, double sampleRate) noexcept;
    void resetPhase() noexcept { phase = 0.0; }
    void setPhase (double newPhase) noexcept; // 0..1 (valores fora são dobrados para dentro)

    // Devolve a amostra atual (entre -1 e 1, com um pequeno sobressinal perto dos saltos da
    // serra e da quadrada) e avança a fase.
    float nextSample() noexcept;

private:
    void updateTable() noexcept;

    Waveform waveform = Waveform::saw;
    double phase = 0.0;     // posição no ciclo, de 0 a 1
    double increment = 0.0; // frequência / sample rate

    // Tabela do nível (mipmap) atual. Iniciada aqui: o primeiro oscilador criado monta o banco.
    const float* table = Wavetables::table (Waveform::saw, 0);
};

// "inline" no header: chamada a cada amostra, para cada cópia de cada oscilador. Com o corpo
// visível aqui, o compilador cola o código dentro do laço da voz em vez de fazer uma chamada de
// função (medido: ~2× mais rápido).
inline float Oscillator::nextSample() noexcept
{
    // Posição fracionária na tabela: índice inteiro + fração entre ele e o próximo.
    const double position = phase * Wavetables::tableSize;
    const int index = std::min (static_cast<int> (position), Wavetables::tableSize - 1);
    const float fraction = static_cast<float> (position - index);

    // Interpolação linear: anda em linha reta da amostra atual até a próxima.
    // (A tabela tem uma amostra extra no fim, então table[index + 1] sempre existe.)
    const float value = table[index] + fraction * (table[index + 1] - table[index]);

    // Volta para [0, 1). O floor só roda quando a fase passa de 1 (uma vez por ciclo): chamado a
    // cada amostra, ele sozinho dobrava o custo do oscilador. Usar floor (e não "- 1") cobre
    // incrementos maiores que 1, que a guarda de Nyquist (RF4) já silencia, mas não podem quebrar.
    phase += increment;
    if (phase >= 1.0)
        phase -= std::floor (phase);

    return value;
}
