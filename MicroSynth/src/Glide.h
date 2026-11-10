#pragma once

#include <algorithm>
#include <cmath>

// Glide (portamento) do modo Mono (RF7): desliza a altura da nota anterior para a nova.
//
// A altura é medida em "notas" (passos da EDO), não em Hz. Andar em linha reta em passos
// equivale a uma curva exponencial em Hz, que é como o ouvido percebe um glide uniforme.
// O tempo é constante: um salto de 1 passo ou de 12 passos leva o mesmo tempo.
//
// C++ puro, sem alocação: seguro na thread de áudio.
class Glide
{
public:
    // Duração do glide. Chamado a cada bloco (barato: só uma conta).
    void setTime (double seconds, double sampleRate) noexcept
    {
        glideSamples = static_cast<int> (std::lround (std::max (0.0, seconds) * std::max (0.0, sampleRate)));
    }

    // Vai direto para a altura, sem deslizar (nota nova numa voz parada).
    void jumpTo (double pitch) noexcept
    {
        current = target = pitch;
        step = 0.0;
        samplesLeft = 0;
    }

    // Desliza da altura atual (mesmo que esteja no meio de outro glide) até a nova.
    void glideTo (double pitch) noexcept
    {
        if (glideSamples <= 0)
        {
            jumpTo (pitch);
            return;
        }

        target = pitch;
        samplesLeft = glideSamples;
        step = (target - current) / samplesLeft;
    }

    bool isGliding() const noexcept { return samplesLeft > 0; }
    double getCurrent() const noexcept { return current; }

    // Avança uma amostra e devolve a altura atual.
    double next() noexcept
    {
        if (samplesLeft > 0)
        {
            --samplesLeft;
            // Na última amostra usa o alvo exato, sem erro acumulado de arredondamento.
            current = samplesLeft == 0 ? target : current + step;
        }

        return current;
    }

private:
    double current = 0.0;
    double target  = 0.0;
    double step    = 0.0;
    int samplesLeft  = 0;
    int glideSamples = 0;
};
