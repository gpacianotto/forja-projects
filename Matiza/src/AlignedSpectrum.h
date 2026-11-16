#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "Waveform.h"
#include "Wavetables.h"

// Harmônicos alinhados à escala (RF21). C++ puro, sem JUCE e sem alocação: testável e seguro
// na thread de áudio.
//
// Um som "afinado" tem harmônicos nos múltiplos inteiros da fundamental (1×, 2×, 3×…). Numa
// escala como a 7 EDO, os harmônicos das notas de um acorde ficam quase juntos, mas não
// exatamente, e batem (soam ásperos). Aqui cada harmônico é puxado para a altura mais próxima
// da afinação do teclado: os harmônicos das notas do acorde passam a cair exatamente uns sobre
// os outros, e o batimento some.
//
// Este arquivo só faz a matemática: para onde vai cada harmônico (Ratios) e com que volume e
// fase ele soa em cada forma de onda (Recipe). Quem toca as senoides é a voz (etapa 23).
namespace AlignedSpectrum
{
    // Harmônicos tocados no motor aditivo; do 25º para cima, o wavetable "do resto" cuida.
    inline constexpr int numPartials = 24;

    // Para de procurar a altura depois de tantos steps (escalas muito densas ou estranhas).
    inline constexpr int maxStepsSearched = 4096;

    struct Ratios
    {
        // Razão de frequência de cada harmônico em relação à fundamental; ratio[h - 1] é o
        // harmônico h. Sem alinhamento, ratio[h - 1] = h.
        std::array<double, numPartials> ratio {};

        // Quantos harmônicos (1..alignedCount) foram alinhados. Os de cima ficam naturais: o
        // próximo cairia na mesma altura que o anterior (limite de colisão), a afinação acabou
        // (fim da tabela) ou não há mais harmônicos no motor aditivo. Não depende do Align,
        // para nada pular ao girar o knob.
        int alignedCount = 0;

        bool operator== (const Ratios&) const = default;
    };

    // centsOfStep(k): altura do step k em cents, contada a partir do step do oscilador
    // (centsOfStep(0) = 0), crescente com k. Pode devolver NaN onde a afinação acaba.
    // A voz monta essa função com o TuningEngine (EDO, afinação desenhada ou master), como no
    // ScaleLines do RF19.
    //
    // amount: 0 = harmônicos naturais; 1 = na altura da escala; no meio, interpola em cents.
    //
    // "template": como um método genérico do Java; CentsOfStep é qualquer coisa chamável que
    // recebe int e devolve double (uma lambda, por exemplo).
    template <typename CentsOfStep>
    Ratios align (CentsOfStep centsOfStep, double amount)
    {
        amount = std::clamp (amount, 0.0, 1.0);

        Ratios out;
        for (int h = 1; h <= numPartials; ++h)
            out.ratio[static_cast<size_t> (h - 1)] = h;

        // Os harmônicos sobem sempre, então a busca do step continua de onde parou: no total,
        // anda no máximo até a altura do 24º harmônico (~5500 ¢), não do zero a cada vez.
        int stepBelow = 0;       // maior step com altura <= harmônico atual
        double centsBelow = 0.0; // altura desse step
        int previousTarget = -1; // step em que caiu o harmônico anterior

        for (int h = 1; h <= numPartials; ++h)
        {
            const double natural = 1200.0 * std::log2 (static_cast<double> (h));

            // Anda para cima até o próximo step passar do harmônico.
            double centsAbove = 0.0;
            bool found = false;
            while (stepBelow < maxStepsSearched)
            {
                centsAbove = centsOfStep (stepBelow + 1);
                if (! std::isfinite (centsAbove))
                    break; // a afinação acabou
                if (centsAbove > natural)
                {
                    found = true;
                    break;
                }
                ++stepBelow;
                centsBelow = centsAbove;
            }
            if (! found)
                break;

            // O step mais perto (empate fica com o de baixo).
            const bool belowIsNearer = natural - centsBelow <= centsAbove - natural;
            const int target = belowIsNearer ? stepBelow : stepBelow + 1;
            if (target == previousTarget)
                break; // colisão: este harmônico cairia na mesma altura que o anterior

            const double targetCents = belowIsNearer ? centsBelow : centsAbove;
            out.ratio[static_cast<size_t> (h - 1)] = std::exp2 ((natural + amount * (targetCents - natural)) / 1200.0);
            out.alignedCount = h;
            previousTarget = target;
        }

        return out;
    }

    // "Receita" de uma forma de onda: volume e fase dos 24 primeiros harmônicos. O harmônico h
    // soa como sinAmp × sen(fase) + cosAmp × cos(fase). A síntese por recorrência calcula o
    // seno e o cosseno de cada harmônico de graça, então a fase custa só uma multiplicação.
    struct Recipe
    {
        std::array<float, numPartials> sinAmp {};
        std::array<float, numPartials> cosAmp {};

        bool operator== (const Recipe&) const = default;
    };

    // Formas prontas: os mesmos coeficientes que montam o banco de wavetables (só senos).
    inline Recipe builtinRecipe (Waveform waveform)
    {
        Recipe recipe;
        for (int h = 1; h <= numPartials; ++h)
            recipe.sinAmp[static_cast<size_t> (h - 1)] = static_cast<float> (Wavetables::harmonicAmplitude (waveform, h));
        return recipe;
    }

    // Forma desenhada (RF15): série de Fourier do ciclo (tableSize amostras), só nos 24
    // primeiros harmônicos. Conta direta (24 × 2048 multiplicações por coeficiente), rápida o
    // bastante fora da thread de áudio, junto com os mipmaps.
    inline Recipe recipeFromCycle (const float* cycle)
    {
        constexpr int n = Wavetables::tableSize;
        constexpr double twoPi = 2.0 * std::numbers::pi;

        Recipe recipe;
        for (int h = 1; h <= numPartials; ++h)
        {
            double sinSum = 0.0, cosSum = 0.0;
            for (int i = 0; i < n; ++i)
            {
                // (h × i) mod n: mesma posição no ciclo, sem perder precisão com ângulos grandes.
                const double angle = twoPi * static_cast<double> ((h * i) % n) / n;
                sinSum += cycle[i] * std::sin (angle);
                cosSum += cycle[i] * std::cos (angle);
            }
            recipe.sinAmp[static_cast<size_t> (h - 1)] = static_cast<float> (2.0 * sinSum / n);
            recipe.cosAmp[static_cast<size_t> (h - 1)] = static_cast<float> (2.0 * cosSum / n);
        }
        return recipe;
    }
}
