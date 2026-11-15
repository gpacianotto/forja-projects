#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "StepMath.h"

// Matemática do gráfico dos steps quando o Matiza segue o master do MTS-ESP (RF18).
// C++ puro, sem JUCE: testável sozinha.
//
// O master entrega 128 frequências (uma por nota MIDI), não a "estrutura" da escala. O
// gráfico reconstrói uma volta da escala assim (regra do Spec):
//   - step 0 = a tecla de referência do master (ou a Ref Note do Matiza, se ele não informar);
//   - uma volta (Circle) ou a largura (Line) = o período da escala (normalmente a oitava);
//   - as linhas são as notas a partir do step 0 cuja altura fica dentro de um período;
//   - uma nota tocada acende a linha (nota - step 0) mod número de linhas.
namespace MasterScale
{
    inline constexpr int numMidiNotes = 128;
    inline constexpr int maxLines     = 72; // como o máximo do EDO: o gráfico não desenha mais que isso

    struct Layout
    {
        int    refNote     = 69;     // nota MIDI do step 0
        double refHz       = 440.0;  // frequência do step 0
        double periodCents = 1200.0; // tamanho de uma volta, em cents
        int    numLines    = 1;
        std::array<double, maxLines> lineCents {}; // posição de cada linha, 0 <= c < periodCents

        bool operator== (const Layout&) const = default; // compara campo a campo (C++20)
    };

    // Frequência inválida (zero, negativa, NaN) vale como 12-TET com A = 440 Hz, como no motor.
    inline double safeHz (const std::array<double, numMidiNotes>& hz, int note) noexcept
    {
        const double value = hz[static_cast<std::size_t> (note)];
        return std::isfinite (value) && value > 0.0 ? value : 440.0 * std::exp2 ((note - 69) / 12.0);
    }

    // Leva cents para dentro de um período: 0 <= resultado < period.
    inline double wrap (double cents, double period) noexcept
    {
        const double wrapped = std::fmod (cents, period);
        const double result  = wrapped < 0.0 ? wrapped + period : wrapped;
        return result >= period ? 0.0 : result;
    }

    // refKey: o que o master informa (-1 ou fora de 0..127 = não informou).
    // periodRatio: razão do período (2,0 = oitava); inválido vale como oitava.
    inline Layout build (const std::array<double, numMidiNotes>& hz, int refKey, double periodRatio,
                         int fallbackRefNote) noexcept
    {
        Layout layout;
        layout.refNote = (refKey >= 0 && refKey < numMidiNotes) ? refKey
                                                                : std::clamp (fallbackRefNote, 0, numMidiNotes - 1);
        layout.refHz = safeHz (hz, layout.refNote);
        layout.periodCents = (std::isfinite (periodRatio) && periodRatio > 1.0) ? 1200.0 * std::log2 (periodRatio)
                                                                                : 1200.0;

        const double refCents = 1200.0 * std::log2 (layout.refHz);
        layout.numLines = 0;

        for (int n = layout.refNote; n < numMidiNotes && layout.numLines < maxLines; ++n)
        {
            const double cents = 1200.0 * std::log2 (safeHz (hz, n)) - refCents;

            // Chegou ao step 0 do período seguinte (com folga para arredondamento).
            if (n > layout.refNote && cents >= layout.periodCents - 1.0e-6)
                break;

            // Tabelas fora de ordem (raro) caem dentro do período, em vez de sumir do gráfico.
            layout.lineCents[static_cast<std::size_t> (layout.numLines++)] = wrap (cents, layout.periodCents);
        }

        layout.numLines = std::max (1, layout.numLines);
        return layout;
    }

    // Linha e período de uma nota MIDI (como StepMath::locate, com "edo" = número de linhas).
    inline StepMath::NotePosition locate (int midiNote, const Layout& layout) noexcept
    {
        return StepMath::locate (midiNote, layout.refNote, layout.numLines);
    }

    // Altura de uma nota em cents dentro do período, contada a partir do step 0.
    inline double noteCentsInPeriod (const std::array<double, numMidiNotes>& hz, int midiNote,
                                     const Layout& layout) noexcept
    {
        const int note = std::clamp (midiNote, 0, numMidiNotes - 1);
        return wrap (1200.0 * std::log2 (safeHz (hz, note) / layout.refHz), layout.periodCents);
    }

    // O desenho do gráfico trabalha numa volta de 0 a 1200 (StepMath). Com um período que não
    // é a oitava (ex.: Bohlen-Pierce, 3:1 = 1902 ¢), as posições são esticadas para caber.
    inline double toDisplay (double cents, const Layout& layout) noexcept
    {
        return cents * StepMath::centsPerOctave / layout.periodCents;
    }

    inline double fromDisplay (double displayCents, const Layout& layout) noexcept
    {
        return displayCents * layout.periodCents / StepMath::centsPerOctave;
    }
}
