#pragma once

#include <array>
#include <cmath>

#include "AlignedSpectrum.h"
#include "TuningEngine.h"

// O que o gráfico dos harmônicos (RF21, etapa 24) mostra: onde cada um dos 24 harmônicos está
// com o Align atual, com que volume, e as alturas da escala por cima da nota. C++ puro, sem
// JUCE: testável. A interface monta isto num timer e só redesenha quando muda.
//
// Usa a MESMA conta da voz (MatizaVoice::updateAlignedRatios): a altura do oscilador na escala
// (nota + steps) e TuningEngine::centsFromBase para cada step acima dela. Assim o gráfico
// mostra exatamente o que toca, na EDO, na afinação desenhada (RF17) e com o master (RF18).
namespace HarmonicsView
{
    // Eixo do gráfico: da fundamental (0 ¢) até um pouco acima do 24º harmônico (~5502 ¢).
    inline const double maxCents = 1200.0 * std::log2 (AlignedSpectrum::numPartials + 0.5);

    // 72 EDO tem ~330 alturas no eixo; folga para afinações desenhadas e masters densos.
    inline constexpr int maxLines = 512;

    struct Snapshot
    {
        AlignedSpectrum::Ratios ratios;  // posição de cada harmônico (com o Align atual)
        AlignedSpectrum::Recipe recipe;  // volume e fase de cada harmônico (forma de onda)
        double align = 0.0;              // 0..1

        int lineCount = 0;
        std::array<double, maxLines> lineCents {}; // alturas da escala acima da nota, em cents

        // Para o título.
        int oscIndex = 0;  // 0..2
        int waveform = 0;  // índice do enum Waveform
        int note = 60;     // nota MIDI de referência
        bool noteHeld = false;
        bool master = false;

        bool operator== (const Snapshot&) const = default;
    };

    // Volume de um harmônico (junta a parte em seno e a em cosseno da receita).
    inline double amplitude (const AlignedSpectrum::Recipe& recipe, int h)
    {
        const auto i = static_cast<size_t> (h - 1);
        return std::hypot (static_cast<double> (recipe.sinAmp[i]), static_cast<double> (recipe.cosAmp[i]));
    }

    // Preenche ratios e as linhas da escala. Os campos do título ficam com quem chama.
    inline void compute (Snapshot& out, const TuningEngine& tuning, int note, int oscSteps, double align)
    {
        out.align = align;

        const double n = note;
        const double base = tuning.centsFromBase (n, 0.0, oscSteps);
        auto centsOfStep = [&] (int k) { return tuning.centsFromBase (n, 0.0, oscSteps + k) - base; };

        out.ratios = AlignedSpectrum::align (centsOfStep, align);

        out.lineCount = 0;
        out.lineCents.fill (0.0);
        for (int k = 1; out.lineCount < maxLines && k <= AlignedSpectrum::maxStepsSearched; ++k)
        {
            const double cents = centsOfStep (k);
            if (! std::isfinite (cents) || cents > maxCents)
                break;
            out.lineCents[static_cast<size_t> (out.lineCount++)] = cents;
        }
    }
}
