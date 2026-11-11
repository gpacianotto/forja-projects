#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>

// LFO (oscilador de baixa frequência) do Matiza (RF11). C++ puro, sem JUCE, sem alocação:
// seguro na thread de áudio e testável sozinho.
//
// Diferente do oscilador de áudio, o LFO não toca som: ele gera uma curva lenta (de -1 a +1)
// que a matriz de modulação usa para mexer em outros parâmetros (vibrato, wah, tremolo...).
// Por ser lento, não precisa de anti-aliasing (wavetable com mipmaps, como os osciladores):
// o aliasing só importa em frequências audíveis.

// ATENÇÃO: a ordem é o valor salvo nos sets. Nunca reordenar nem remover.
enum class LfoShape
{
    sine = 0,
    triangle,
    sawUp,
    sawDown,
    square,
    sampleHold,
    custom, // forma desenhada (RF15)
    count
};

inline constexpr std::array<const char*, static_cast<std::size_t> (LfoShape::count)> lfoShapeNames {
    "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "Sample & Hold", "Custom"
};

// Divisões de tempo do modo Sync, em semínimas (beats). T = tercina, D = pontuada.
// Mesma regra: ordem fixa para sempre.
inline constexpr std::array<const char*, 13> lfoDivisionNames {
    "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/8D", "1/4T", "1/4", "1/4D", "1/2", "1 Bar", "2 Bars", "4 Bars"
};
inline constexpr std::array<double, 13> lfoDivisionBeats {
    0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0, 4.0, 8.0, 16.0
};
inline constexpr int lfoDefaultDivision = 7; // 1/4

class Lfo
{
public:
    // Frequência de um LFO sincronizado: um ciclo dura "beats" semínimas no tempo "bpm".
    static double syncedHz (double bpm, double beats) noexcept
    {
        return beats > 0.0 ? (bpm / 60.0) / beats : 0.0;
    }

    void setShape (LfoShape newShape) noexcept { shape = newShape; }

    // Forma desenhada (RF15): customTableSize + 1 amostras, a última sendo o fim do ciclo.
    // O dono dos dados é o CustomShapes; o ponteiro vale até o próximo bloco de áudio.
    static constexpr int customTableSize = 2048;
    void setCustomTable (const float* table) noexcept { customTable = table; }

    // Semente do Sample & Hold: vozes com sementes diferentes sorteiam valores diferentes.
    void setSeed (std::uint64_t newSeed) noexcept { seed = newSeed; }

    // Posiciona o LFO: fase de 0 a 1 dentro do ciclo número "cycle".
    void setPosition (double newPhase, std::int64_t newCycle) noexcept
    {
        phase = newPhase - std::floor (newPhase);
        cycle = newCycle + static_cast<std::int64_t> (std::floor (newPhase));
    }

    void reset() noexcept { setPosition (0.0, 0); }

    // Anda "cycles" ciclos (ex.: hz / sampleRate para uma amostra).
    void advance (double cycles) noexcept
    {
        phase += cycles;

        if (phase >= 1.0)
        {
            const double whole = std::floor (phase);
            phase -= whole;
            cycle += static_cast<std::int64_t> (whole);
        }
    }

    double getPhase() const noexcept { return phase; }
    std::int64_t getCycle() const noexcept { return cycle; }

    // Valor atual, de -1 a +1.
    float value() const noexcept
    {
        if (shape == LfoShape::custom)
            return customValue (customTable, phase);

        return valueAt (shape, phase, cycle, seed);
    }

    // Lê a tabela desenhada na fase (0..1), com interpolação linear. Sem tabela, 0.
    static float customValue (const float* table, double phase) noexcept
    {
        if (table == nullptr)
            return 0.0f;

        const double position = phase * customTableSize;
        const int index = position >= customTableSize ? customTableSize - 1 : static_cast<int> (position);
        const float fraction = static_cast<float> (position - index);
        return table[index] + fraction * (table[index + 1] - table[index]);
    }

    static float valueAt (LfoShape shape, double phase, std::int64_t cycle, std::uint64_t seed) noexcept
    {
        double v = 0.0;

        switch (shape)
        {
            case LfoShape::sine:     v = std::sin (2.0 * std::numbers::pi * phase); break;
            // Triângulo começando em 0 e subindo, como a senoide.
            case LfoShape::triangle: v = phase < 0.25 ? 4.0 * phase
                                       : phase < 0.75 ? 2.0 - 4.0 * phase
                                                      : 4.0 * phase - 4.0; break;
            case LfoShape::sawUp:    v = 2.0 * phase - 1.0; break;
            case LfoShape::sawDown:  v = 1.0 - 2.0 * phase; break;
            case LfoShape::square:   v = phase < 0.5 ? 1.0 : -1.0; break;
            case LfoShape::sampleHold: v = randomBipolar (seed, cycle); break;
            case LfoShape::custom:   break; // precisa da tabela: ver value()
            case LfoShape::count:    break;
        }

        return static_cast<float> (v);
    }

    // Número "aleatório" de -1 a +1 calculado a partir da semente e do ciclo (hash SplitMix64).
    // Determinístico: o mesmo ciclo dá sempre o mesmo valor, então vozes no modo Free (mesma
    // semente, mesmo ciclo) sorteiam o mesmo valor, e os testes são reproduzíveis.
    static double randomBipolar (std::uint64_t seed, std::int64_t cycle) noexcept
    {
        std::uint64_t z = seed + static_cast<std::uint64_t> (cycle) * 0x9E3779B97F4A7C15ull;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z = z ^ (z >> 31);

        const double unit = static_cast<double> (z >> 11) / static_cast<double> (1ull << 53); // 0..1
        return unit * 2.0 - 1.0;
    }

private:
    LfoShape shape = LfoShape::sine;
    double phase = 0.0;       // 0..1 dentro do ciclo
    std::int64_t cycle = 0;   // quantos ciclos completos já passaram (usado pelo Sample & Hold)
    std::uint64_t seed = 0;
    const float* customTable = nullptr;
};
