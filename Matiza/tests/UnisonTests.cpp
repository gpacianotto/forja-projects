#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

#include "Unison.h"

using Catch::Matchers::WithinAbs;
using namespace Unison;

TEST_CASE ("Unison: uma copia soa como o oscilador sem unison", "[unison]")
{
    const auto layout = computeLayout (1, 25.0f, 0.5f, 1.0f);

    REQUIRE (layout.count == 1);
    REQUIRE (layout.detuneCents[0] == 0.0f);
    REQUIRE (layout.gainLeft[0] == 1.0f);
    REQUIRE (layout.gainRight[0] == 1.0f);
}

TEST_CASE ("Unison: numero de copias e limitado a 1..8", "[unison]")
{
    REQUIRE (computeLayout (0, 25.0f, 0.5f, 1.0f).count == 1);
    REQUIRE (computeLayout (-3, 25.0f, 0.5f, 1.0f).count == 1);
    REQUIRE (computeLayout (20, 25.0f, 0.5f, 1.0f).count == maxVoices);
}

TEST_CASE ("Unison: detune espalhado de forma simetrica entre -detune e +detune", "[unison]")
{
    const auto layout = computeLayout (7, 30.0f, 0.5f, 1.0f);

    REQUIRE_THAT (layout.detuneCents[0], WithinAbs (-30.0, 1e-5));
    REQUIRE_THAT (layout.detuneCents[3], WithinAbs (0.0, 1e-5));
    REQUIRE_THAT (layout.detuneCents[6], WithinAbs (30.0, 1e-5));

    for (int k = 0; k < layout.count; ++k)
        REQUIRE_THAT (layout.detuneCents[k], WithinAbs (-layout.detuneCents[layout.count - 1 - k], 1e-5));

    // Com número par, ninguém fica exatamente na nota, mas o par do meio fica simétrico.
    const auto even = computeLayout (4, 30.0f, 0.5f, 1.0f);
    REQUIRE_THAT (even.detuneCents[1], WithinAbs (-10.0, 1e-5));
    REQUIRE_THAT (even.detuneCents[2], WithinAbs (10.0, 1e-5));
}

TEST_CASE ("Unison: copias centrais", "[unison]")
{
    REQUIRE (isCentral (2, 5));
    REQUIRE_FALSE (isCentral (1, 5));
    REQUIRE (isCentral (1, 4));
    REQUIRE (isCentral (2, 4));
    REQUIRE_FALSE (isCentral (0, 4));
}

TEST_CASE ("Unison: potencia total igual a de uma copia, para qualquer numero e blend", "[unison]")
{
    for (int count = 1; count <= maxVoices; ++count)
    {
        for (float blend : { 0.0f, 0.3f, 1.0f })
        {
            // Com width 0, esquerda = direita = ganho "mono" de cada cópia.
            const auto layout = computeLayout (count, 25.0f, blend, 0.0f);
            double power = 0.0;
            for (int k = 0; k < layout.count; ++k)
                power += layout.gainLeft[k] * layout.gainLeft[k];

            REQUIRE_THAT (power, WithinAbs (1.0, 1e-5));
        }
    }
}

TEST_CASE ("Unison: blend 0 deixa so as copias centrais", "[unison]")
{
    const auto layout = computeLayout (5, 25.0f, 0.0f, 0.0f);

    for (int k = 0; k < layout.count; ++k)
    {
        if (k == 2)
            REQUIRE (layout.gainLeft[k] > 0.0f);
        else
            REQUIRE (layout.gainLeft[k] == 0.0f);
    }
}

TEST_CASE ("Unison: width 0 deixa tudo no centro; width 1 abre as pontas", "[unison]")
{
    const auto narrow = computeLayout (6, 25.0f, 1.0f, 0.0f);
    for (int k = 0; k < narrow.count; ++k)
        REQUIRE_THAT (narrow.gainLeft[k], WithinAbs (narrow.gainRight[k], 1e-6));

    const auto wide = computeLayout (6, 25.0f, 1.0f, 1.0f);
    REQUIRE_THAT (wide.gainRight[0], WithinAbs (0.0, 1e-6)); // mais grave: toda à esquerda
    REQUIRE_THAT (wide.gainLeft[5],  WithinAbs (0.0, 1e-6)); // mais aguda: toda à direita
    REQUIRE (wide.gainLeft[0] > wide.gainLeft[2]);

    // Pan de potência constante: L² + R² não depende da posição.
    for (int k = 0; k < wide.count; ++k)
    {
        const double p  = wide.gainLeft[k] * wide.gainLeft[k] + wide.gainRight[k] * wide.gainRight[k];
        const double p0 = wide.gainLeft[0] * wide.gainLeft[0] + wide.gainRight[0] * wide.gainRight[0];
        REQUIRE_THAT (p, WithinAbs (p0, 1e-5));
    }
}

// --- Gráfico do unison (RF19): altura das barras e tiques da régua L/R ---

TEST_CASE ("Grafico do unison: altura das barras = centrais 1, laterais Blend")
{
    // 4 cópias: as duas do meio são centrais.
    CHECK (Unison::relativeLevel (0, 4, 0.3f) == 0.3f);
    CHECK (Unison::relativeLevel (1, 4, 0.3f) == 1.0f);
    CHECK (Unison::relativeLevel (2, 4, 0.3f) == 1.0f);
    CHECK (Unison::relativeLevel (3, 4, 0.3f) == 0.3f);

    // Blend 0: só as centrais aparecem; fora da faixa, limitado.
    CHECK (Unison::relativeLevel (0, 5, 0.0f) == 0.0f);
    CHECK (Unison::relativeLevel (2, 5, 0.0f) == 1.0f);
    CHECK (Unison::relativeLevel (0, 5, 2.0f) == 1.0f);
}

TEST_CASE ("Grafico do unison: pan de cada copia na regua L/R")
{
    CHECK (Unison::pan (0, 1, 1.0f) == 0.0f);   // uma cópia: centro
    CHECK (Unison::pan (0, 3, 1.0f) == -1.0f);  // ponta esquerda
    CHECK (Unison::pan (1, 3, 1.0f) == 0.0f);
    CHECK (Unison::pan (2, 3, 1.0f) == 1.0f);   // ponta direita
    CHECK (Unison::pan (2, 3, 0.5f) == 0.5f);   // Width 50 %
    CHECK (Unison::pan (0, 3, 0.0f) == 0.0f);   // Width 0: tudo no centro
}

TEST_CASE ("Grafico do unison: a conta do grafico e a do audio")
{
    // O pan do gráfico tem que bater com os ganhos L/R que o áudio usa.
    const auto layout = Unison::computeLayout (5, 30.0f, 0.6f, 0.8f);
    for (int k = 0; k < layout.count; ++k)
    {
        const auto i = static_cast<std::size_t> (k);
        const float angle = std::atan2 (layout.gainRight[i], layout.gainLeft[i]);
        const float panFromGains = angle * 4.0f / std::numbers::pi_v<float> - 1.0f;
        CHECK_THAT (panFromGains, Catch::Matchers::WithinAbs (Unison::pan (k, 5, 0.8f), 1e-5));
    }
}
