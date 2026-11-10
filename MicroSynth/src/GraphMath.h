#pragma once

#include <algorithm>
#include <cmath>

// Matemática dos gráficos da interface (RF9). C++ puro, sem JUCE: testável sozinha.
namespace GraphMath
{
    // Ganho (em dB) do filtro passa-baixa de 2 polos na frequência freqHz.
    //
    // O StateVariableTPTFilter do JUCE imita o filtro analógico
    //     H(s) = 1 / (s² + s/Q + 1),   com s = j·f/fc
    // em que Q é o parâmetro "resonance". O módulo fica
    //     |H| = 1 / sqrt((1 - x²)² + (x/Q)²),   x = f/fc
    // Abaixo do cutoff ≈ 0 dB; no cutoff = 20·log10(Q); acima cai 12 dB por oitava.
    // (Perto de Nyquist o filtro digital se afasta um pouco disso, mas o desenho serve.)
    inline double lowpassMagnitudeDb (double freqHz, double cutoffHz, double q)
    {
        const double x = freqHz / std::max (1.0, cutoffHz);
        const double qSafe = std::max (0.01, q);
        const double a = 1.0 - x * x;
        const double b = x / qSafe;
        const double magnitude = 1.0 / std::sqrt (a * a + b * b);
        return 20.0 * std::log10 (std::max (1.0e-9, magnitude));
    }

    // Onde cada fase do ADSR termina no eixo horizontal do desenho, de 0 a 1
    // (o release termina sempre em 1).
    struct EnvelopeLayout
    {
        double attackEnd  = 0.0;
        double decayEnd   = 0.0;
        double sustainEnd = 0.0;
    };

    // Fração da largura reservada para o sustain, que não tem duração (dura enquanto a tecla
    // estiver pressionada).
    inline constexpr double sustainWidth = 0.2;

    // Larguras proporcionais à raiz quadrada dos tempos: com escala linear, um attack de 5 ms
    // sumiria ao lado de um release de 10 s.
    inline EnvelopeLayout layoutEnvelope (double attackSec, double decaySec, double releaseSec)
    {
        const double wa = std::sqrt (std::max (0.0, attackSec));
        const double wd = std::sqrt (std::max (0.0, decaySec));
        const double wr = std::sqrt (std::max (0.0, releaseSec));
        const double total = wa + wd + wr;

        // Tudo zero: divide igualmente para o desenho não degenerar.
        const double timed = 1.0 - sustainWidth;
        const double fa = total > 0.0 ? wa / total : 1.0 / 3.0;
        const double fd = total > 0.0 ? wd / total : 1.0 / 3.0;

        EnvelopeLayout layout;
        layout.attackEnd  = fa * timed;
        layout.decayEnd   = (fa + fd) * timed;
        layout.sustainEnd = layout.decayEnd + sustainWidth;
        return layout;
    }

    // Em que fase está o envelope da nota mostrada na interface.
    enum class EnvelopeStage
    {
        idle = 0, // nenhuma nota tocando: sem ponto no gráfico
        held,     // tecla pressionada: attack, decay ou sustain, conforme o tempo
        released  // tecla solta: release
    };

    // Posição horizontal (0 a 1) do ponto que mostra onde o envelope está.
    // secondsInStage conta desde o note-on (held) ou desde o note-off (released).
    inline double envelopePlayheadX (const EnvelopeLayout& layout, EnvelopeStage stage, double secondsInStage,
                                     double attackSec, double decaySec, double releaseSec)
    {
        auto progress = [] (double t, double duration)
        {
            return duration > 0.0 ? std::clamp (t / duration, 0.0, 1.0) : 1.0;
        };

        switch (stage)
        {
            case EnvelopeStage::idle:
                return 0.0;

            case EnvelopeStage::held:
                if (secondsInStage < attackSec)
                    return layout.attackEnd * progress (secondsInStage, attackSec);

                if (secondsInStage < attackSec + decaySec)
                    return layout.attackEnd
                         + (layout.decayEnd - layout.attackEnd) * progress (secondsInStage - attackSec, decaySec);

                return layout.decayEnd; // sustain: o ponto fica no começo do patamar

            case EnvelopeStage::released:
                return layout.sustainEnd + (1.0 - layout.sustainEnd) * progress (secondsInStage, releaseSec);
        }

        return 0.0;
    }
}
