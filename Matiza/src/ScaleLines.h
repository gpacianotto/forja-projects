#pragma once

#include <array>
#include <cmath>

// Linhas da escala atual em volta de uma nota, para o gráfico do unison (RF19): os steps
// vizinhos (−1, +1, −2, +2…) que caem dentro de ±rangeCents. C++ puro, sem JUCE: testável.
//
// Quem chama diz quanto vale cada step (EDO, afinação desenhada do RF17 ou master do RF18) com
// uma função centsOfStep(k) → cents do step k em relação ao step 0. Assim este arquivo não
// precisa conhecer nenhuma das três afinações.
namespace ScaleLines
{
    inline constexpr int maxLines    = 32;  // 72 EDO tem 12 linhas em ±100 ¢; folga de sobra
    inline constexpr int maxStepsOut = 128; // para de procurar depois disso (escalas estranhas)

    struct Lines
    {
        int count = 0;
        std::array<double, maxLines> cents {}; // só os primeiros `count` valem; nunca o 0

        bool operator== (const Lines&) const = default; // compara campo a campo (C++20)
    };

    // "template": como um método genérico do Java; CentsOfStep é qualquer coisa chamável
    // (uma lambda, por exemplo) que recebe int e devolve double.
    template <typename CentsOfStep>
    Lines around (CentsOfStep centsOfStep, double rangeCents)
    {
        Lines lines;

        // Para cima (+1, +2…) e para baixo (−1, −2…), parando no primeiro step fora da faixa.
        for (const int direction : { +1, -1 })
        {
            for (int k = 1; k <= maxStepsOut && lines.count < maxLines; ++k)
            {
                const double cents = centsOfStep (direction * k);
                if (! std::isfinite (cents) || std::abs (cents) > rangeCents)
                    break;

                if (std::abs (cents) > 1.0e-9) // um step colado no 0 não vira linha
                    lines.cents[static_cast<std::size_t> (lines.count++)] = cents;
            }
        }

        return lines;
    }
}
