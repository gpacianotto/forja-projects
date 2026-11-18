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

    // Mipmaps da forma desenhada (RF15), usados quando a forma é Custom. O dono dos dados é o
    // CustomShapes; o ponteiro vale até o próximo bloco de áudio. residualLevels: as tabelas
    // "do resto" da mesma forma (RF21), ou nullptr.
    void setCustomTables (const float* levels, const float* residualLevels = nullptr) noexcept;

    // Harmônicos alinhados (RF21): true = lê as tabelas "do resto" (sem os 24 primeiros
    // harmônicos, que tocam no PartialBank); false = a onda completa.
    void setResidual (bool useResidual) noexcept;
    void setFrequency (double hz, double sampleRate) noexcept;
    void resetPhase() noexcept { phase = 0.0; }
    void setPhase (double newPhase) noexcept; // 0..1 (valores fora são dobrados para dentro)
    double getPhase() const noexcept { return phase; }

    // Devolve a amostra atual (entre -1 e 1, com um pequeno sobressinal perto dos saltos da
    // serra e da quadrada) e avança a fase.
    //
    // fundamentalCut (RF22, botão No Fund): quanto da fundamental (1º harmônico) tirar da onda,
    // de 0 (onda inteira) a 1 (sem a fundamental). A voz passa um valor que anda em rampa ao
    // ligar ou desligar o botão. Lendo as tabelas "do resto" (RF21) não há fundamental na
    // tabela (ela toca no PartialBank), então o valor é ignorado.
    float nextSample (float fundamentalCut = 0.0f) noexcept;

private:
    void updateTable() noexcept;

    Waveform waveform = Waveform::saw;
    double phase = 0.0;     // posição no ciclo, de 0 a 1
    double increment = 0.0; // frequência / sample rate

    // Tabela do nível (mipmap) atual. Iniciada aqui: o primeiro oscilador criado monta o banco.
    const float* table = Wavetables::table (Waveform::saw, 0);
    const float* customLevels = nullptr;
    const float* customResidualLevels = nullptr;
    bool residual = false;

    // Só a fundamental da forma atual (RF22): o último mipmap, que guarda apenas o 1º harmônico
    // (Wavetables::maxHarmonic (20) = 1), com a mesma amplitude e fase que ele tem nos outros
    // níveis. nullptr lendo as tabelas "do resto".
    const float* fundamental = Wavetables::table (Waveform::saw, Wavetables::numLevels - 1);
};

// "inline" no header: chamada a cada amostra, para cada cópia de cada oscilador. Com o corpo
// visível aqui, o compilador cola o código dentro do laço da voz em vez de fazer uma chamada de
// função (medido: ~2× mais rápido).
inline float Oscillator::nextSample (float fundamentalCut) noexcept
{
    // Posição fracionária na tabela: índice inteiro + fração entre ele e o próximo.
    const double position = phase * Wavetables::tableSize;
    const int index = std::min (static_cast<int> (position), Wavetables::tableSize - 1);
    const float fraction = static_cast<float> (position - index);

    // Interpolação linear: anda em linha reta da amostra atual até a próxima.
    // (A tabela tem uma amostra extra no fim, então table[index + 1] sempre existe.)
    float value = table[index] + fraction * (table[index + 1] - table[index]);

    // RF22: subtrai a fundamental lida no mesmo índice e com a mesma interpolação. Como a
    // interpolação linear é linear (interpolar a diferença = diferença das interpolações), o
    // 1º harmônico some por inteiro e os outros não mudam. O "if" custa quase nada: o
    // processador acerta a previsão, porque o valor é o mesmo por milhares de amostras.
    if (fundamentalCut != 0.0f && fundamental != nullptr)
        value -= fundamentalCut * (fundamental[index] + fraction * (fundamental[index + 1] - fundamental[index]));

    // Volta para [0, 1). O floor só roda quando a fase passa de 1 (uma vez por ciclo): chamado a
    // cada amostra, ele sozinho dobrava o custo do oscilador. Usar floor (e não "- 1") cobre
    // incrementos maiores que 1, que a guarda de Nyquist (RF4) já silencia, mas não podem quebrar.
    phase += increment;
    if (phase >= 1.0)
        phase -= std::floor (phase);

    return value;
}
