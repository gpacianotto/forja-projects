#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

// Unison (RF12): várias cópias da mesma onda tocando juntas, um pouco desafinadas entre si e
// espalhadas no estéreo. As pequenas diferenças de frequência fazem as cópias "baterem" umas
// contra as outras (o batimento), e o som fica largo e encorpado, como um coro.
//
// Este arquivo só calcula ONDE cada cópia fica (desafinação, volume e pan). Os osciladores em
// si continuam na voz. C++ puro, sem JUCE e sem alocação: pode ser chamado na thread de áudio.
namespace Unison
{
    inline constexpr int maxVoices = 8;

    // Resultado do cálculo para um oscilador. Só os primeiros `count` itens valem.
    struct Layout
    {
        int count = 1;
        std::array<float, maxVoices> detuneCents {}; // desvio de cada cópia em relação à nota
        std::array<float, maxVoices> gainLeft {};    // volume final no canal esquerdo
        std::array<float, maxVoices> gainRight {};   // volume final no canal direito
    };

    // Posição de uma cópia de -1 (a mais grave / mais à esquerda) a +1 (a mais aguda / mais à
    // direita), espalhadas por igual. Com uma cópia só, fica no centro (0).
    inline float position (int index, int count) noexcept
    {
        if (count <= 1)
            return 0.0f;

        return 2.0f * static_cast<float> (index) / static_cast<float> (count - 1) - 1.0f;
    }

    // Cópias centrais: a do meio (número ímpar) ou as duas do meio (número par).
    // O Blend controla o volume das outras em relação a estas.
    inline bool isCentral (int index, int count) noexcept
    {
        if (count % 2 == 1)
            return index == count / 2;

        return index == count / 2 - 1 || index == count / 2;
    }

    // count: 1 a 8 (valores fora da faixa são limitados).
    // detuneCents: desvio das cópias das pontas (as outras ficam entre -detune e +detune).
    // blend e width: 0 a 1.
    inline Layout computeLayout (int count, float detuneCents, float blend, float width) noexcept
    {
        Layout layout;
        layout.count = std::clamp (count, 1, maxVoices);

        // Uma cópia: exatamente o oscilador de antes (sem desvio, centro, volume 1 nos dois lados).
        if (layout.count == 1)
        {
            layout.gainLeft[0]  = 1.0f;
            layout.gainRight[0] = 1.0f;
            return layout;
        }

        blend = std::clamp (blend, 0.0f, 1.0f);
        width = std::clamp (width, 0.0f, 1.0f);

        // Volume de cada cópia antes da normalização.
        std::array<float, maxVoices> gains {};
        float sumOfSquares = 0.0f;

        for (int k = 0; k < layout.count; ++k)
        {
            gains[k] = isCentral (k, layout.count) ? 1.0f : blend;
            sumOfSquares += gains[k] * gains[k];
        }

        // Normaliza pela potência: cópias de fases diferentes somam como ruído (em potência, não
        // em amplitude), então dividir pela raiz da soma dos quadrados mantém o volume parecido
        // com o de uma cópia só, para qualquer número de cópias e qualquer blend.
        const float normalisation = 1.0f / std::sqrt (sumOfSquares);

        for (int k = 0; k < layout.count; ++k)
        {
            const float pos = position (k, layout.count);
            layout.detuneCents[k] = detuneCents * pos;

            // Pan de potência constante: o ângulo vai de 0 (todo à esquerda) a pi/2 (todo à
            // direita). O fator raiz de 2 faz o centro valer 1 nos dois lados, como antes do unison.
            const float pan   = width * pos;
            const float angle = (pan + 1.0f) * static_cast<float> (std::numbers::pi) / 4.0f;
            const float gain  = gains[k] * normalisation * std::numbers::sqrt2_v<float>;

            layout.gainLeft[k]  = gain * std::cos (angle);
            layout.gainRight[k] = gain * std::sin (angle);
        }

        return layout;
    }
}
