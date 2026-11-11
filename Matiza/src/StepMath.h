#pragma once

#include <algorithm>
#include <cmath>

// Matemática do gráfico dos steps da EDO (RF16). C++ puro, sem JUCE: testável sozinha.
//
// Tudo é medido em cents dentro da oitava (0 a 1200), a escala que o ouvido percebe: numa
// EDO de N passos, o step k fica em k × 1200 / N cents, então as linhas ficam equidistantes.
namespace StepMath
{
    inline constexpr double centsPerOctave = 1200.0;

    // Onde uma nota MIDI cai no gráfico: o step dentro da oitava e em qual oitava (contada a
    // partir da Ref Note, que é o step 0 da oitava 0).
    struct NotePosition
    {
        int step   = 0; // 0 .. edo-1
        int octave = 0; // negativo abaixo da Ref Note
    };

    // Divisão "para baixo" (floor): -1 / 12 dá -1, e não 0 como o operador / do C++.
    // Em Java seria Math.floorDiv / Math.floorMod.
    inline int floorDiv (int a, int b) noexcept
    {
        const int q = a / b;
        return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
    }

    // Resto sempre entre 0 e b-1, também para números negativos (o % do C++ daria negativo).
    inline int floorMod (int a, int b) noexcept
    {
        return a - floorDiv (a, b) * b;
    }

    // step = (nota - refNote) mod edo; oitava = floor((nota - refNote) / edo).
    inline NotePosition locate (int midiNote, int refNote, int edo) noexcept
    {
        const int safeEdo = std::max (1, edo);
        const int offset  = midiNote - refNote;
        return { floorMod (offset, safeEdo), floorDiv (offset, safeEdo) };
    }

    // Posição do step k, em cents dentro da oitava.
    inline double stepCents (int step, int edo) noexcept
    {
        return centsPerOctave * step / std::max (1, edo);
    }

    // Leva uma altura em cents (contada a partir da Base Freq, pode ser negativa ou passar de
    // uma oitava) para dentro da oitava: 0 <= resultado < 1200. Usado no ponto que anda com o
    // pitch bend e o glide.
    inline double wrapCents (double cents) noexcept
    {
        const double wrapped = std::fmod (cents, centsPerOctave);
        const double result  = wrapped < 0.0 ? wrapped + centsPerOctave : wrapped;
        return result >= centsPerOctave ? 0.0 : result; // -1e-17 + 1200 arredonda para 1200
    }

    // Frequência de uma posição: baseHz × 2^(oitava + cents/1200).
    inline double toHz (double baseHz, int octave, double cents) noexcept
    {
        return baseHz * std::pow (2.0, octave + cents / centsPerOctave);
    }

    // Com muitos steps os números se embolam: até 24 steps numera todos; até 48, os pares;
    // acima disso, um a cada 4. (Os steps acesos sempre ganham número; quem desenha decide.)
    inline int labelStride (int edo) noexcept
    {
        if (edo <= 24) return 1;
        if (edo <= 48) return 2;
        return 4;
    }

    inline bool shouldLabel (int step, int edo) noexcept
    {
        return step % labelStride (edo) == 0;
    }

    // Visão Circle: ângulo (radianos) de uma posição em cents, com 0 no topo e crescendo no
    // sentido horário. Uma volta (2π) = uma oitava.
    // Na tela (y cresce para baixo): x = centro.x + r·sin(ângulo), y = centro.y - r·cos(ângulo).
    inline double centsToAngle (double cents) noexcept
    {
        constexpr double twoPi = 6.283185307179586;
        return twoPi * cents / centsPerOctave;
    }

    // Visão Line: posição horizontal de 0 (0 ¢) a 1 (1200 ¢).
    inline double centsToFraction (double cents) noexcept
    {
        return cents / centsPerOctave;
    }

    // Brilho de uma linha acesa (0 a 1): fica em 1 enquanto a nota está segurada e, depois de
    // solta, cai em linha reta até 0 em fadeSeconds. Chamado a cada quadro da interface.
    inline float nextGlow (float current, bool held, double frameSeconds, double fadeSeconds) noexcept
    {
        if (held)
            return 1.0f;

        const double drop = fadeSeconds > 0.0 ? frameSeconds / fadeSeconds : 1.0;
        return static_cast<float> (std::max (0.0, current - drop));
    }
}
