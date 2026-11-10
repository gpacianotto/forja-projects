// Testes da matemática dos gráficos (RF9).
// Nomes de TEST_CASE sem acento: o ctest no Windows não os encontra se tiverem.
#include "GraphMath.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using Catch::Matchers::WithinAbs;
using namespace GraphMath;

// ----------------------------------------------------------------------------- filtro

TEST_CASE ("Filtro: bem abaixo do cutoff ~ 0 dB")
{
    CHECK_THAT (lowpassMagnitudeDb (20.0, 8000.0, 0.7071), WithinAbs (0.0, 0.01));
}

TEST_CASE ("Filtro: no cutoff o ganho e 20*log10(Q)")
{
    CHECK_THAT (lowpassMagnitudeDb (1000.0, 1000.0, 1.0 / std::sqrt (2.0)), WithinAbs (-3.01, 0.01));
    CHECK_THAT (lowpassMagnitudeDb (1000.0, 1000.0, 10.0), WithinAbs (20.0, 0.01));
}

TEST_CASE ("Filtro: acima do cutoff cai cerca de 12 dB por oitava")
{
    const double at8x  = lowpassMagnitudeDb (8000.0, 1000.0, 0.7071);
    const double at16x = lowpassMagnitudeDb (16000.0, 1000.0, 0.7071);
    CHECK_THAT (at8x - at16x, WithinAbs (12.0, 0.1));
}

// ----------------------------------------------------------------------------- envelope

TEST_CASE ("Envelope: fases em ordem e sustain com largura fixa")
{
    const auto layout = layoutEnvelope (0.005, 0.2, 0.3);

    CHECK (layout.attackEnd > 0.0);
    CHECK (layout.attackEnd < layout.decayEnd);
    CHECK_THAT (layout.sustainEnd - layout.decayEnd, WithinAbs (sustainWidth, 1e-12));
    CHECK (layout.sustainEnd < 1.0);
}

TEST_CASE ("Envelope: escala de raiz quadrada (4x mais tempo = 2x mais largo)")
{
    const auto layout = layoutEnvelope (0.1, 0.4, 0.0);
    const double attackWidth = layout.attackEnd;
    const double decayWidth  = layout.decayEnd - layout.attackEnd;
    CHECK_THAT (decayWidth / attackWidth, WithinAbs (2.0, 1e-9));
}

TEST_CASE ("Envelope: tempos zerados nao degeneram")
{
    const auto layout = layoutEnvelope (0.0, 0.0, 0.0);
    CHECK_THAT (layout.attackEnd, WithinAbs ((1.0 - sustainWidth) / 3.0, 1e-12));
    CHECK (layout.sustainEnd < 1.0);
}

TEST_CASE ("Envelope: ponto percorre attack, decay, sustain e release")
{
    const double a = 0.1, d = 0.2, r = 0.4;
    const auto layout = layoutEnvelope (a, d, r);

    CHECK (envelopePlayheadX (layout, EnvelopeStage::held, 0.0, a, d, r) == 0.0);
    CHECK_THAT (envelopePlayheadX (layout, EnvelopeStage::held, a / 2, a, d, r), WithinAbs (layout.attackEnd / 2, 1e-12));
    CHECK_THAT (envelopePlayheadX (layout, EnvelopeStage::held, a + d / 2, a, d, r),
                WithinAbs ((layout.attackEnd + layout.decayEnd) / 2, 1e-12));

    // Sustain: fica parado no começo do patamar, por mais tempo que a tecla fique pressionada.
    CHECK (envelopePlayheadX (layout, EnvelopeStage::held, 100.0, a, d, r) == layout.decayEnd);

    CHECK (envelopePlayheadX (layout, EnvelopeStage::released, 0.0, a, d, r) == layout.sustainEnd);
    CHECK_THAT (envelopePlayheadX (layout, EnvelopeStage::released, r / 2, a, d, r),
                WithinAbs ((layout.sustainEnd + 1.0) / 2, 1e-12));
    CHECK (envelopePlayheadX (layout, EnvelopeStage::released, 10.0, a, d, r) == 1.0);
}
