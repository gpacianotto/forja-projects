#pragma once

#include "Waveform.h"

// Banco de wavetables dos osciladores (RF13). C++ puro, sem JUCE.
//
// Um wavetable é um array com UM ciclo da onda. O oscilador anda por ele mais rápido ou mais
// devagar conforme a frequência (como ler um float[] com índice fracionário, em Java).
//
// Problema: uma serra tem harmônicos infinitos. Numa nota aguda, os de cima passariam de
// Nyquist (metade da taxa de amostragem) e voltariam espelhados como notas desafinadas
// (aliasing). Solução: MIPMAPS, várias versões da mesma tabela, cada uma só com os harmônicos
// que cabem abaixo de Nyquist numa faixa de frequências. Aqui são duas versões por oitava.
//
// As tabelas são montadas uma vez, na primeira chamada de table() (o C++ garante que essa
// inicialização é segura entre threads). O primeiro uso acontece quando as vozes são criadas,
// no construtor do processador, então nunca na thread de áudio. Depois disso, só leitura.
namespace Wavetables
{
    inline constexpr int tableSize = 2048; // potência de 2: o índice dá a volta com "& (tableSize - 1)"
    inline constexpr int numLevels = 21;   // 1023 harmônicos no nível 0 ... 1 harmônico no nível 20

    // Maior harmônico guardado no nível `level` (1023, 724, 512, 362, 256 ... 1).
    int maxHarmonic (int level) noexcept;

    // Nível certo para um oscilador que anda `increment` ciclos por amostra (frequência /
    // taxa de amostragem): o de mais harmônicos cujo maior harmônico ainda fica abaixo de
    // Nyquist, ou seja, maxHarmonic (nível) × increment <= 0,5.
    int levelFor (double increment) noexcept;

    // Tabela de um nível: tableSize + 1 amostras (a última repete a primeira, para a
    // interpolação ler a amostra seguinte sem testar a volta).
    const float* table (Waveform waveform, int level) noexcept;
}
