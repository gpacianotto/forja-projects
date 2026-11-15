// Testes do gráfico dos steps com a escala do master do MTS-ESP (RF18).
// Nomes de TEST_CASE sem acento: o ctest no Windows não os encontra se tiverem.
#include "MasterScale.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using Catch::Matchers::WithinAbs;

namespace
{
    using Table = std::array<double, MasterScale::numMidiNotes>;

    // EDO com a nota `ref` em refHz.
    Table edoTable (int edo, int ref = 60, double refHz = 261.6255653, double periodCents = 1200.0)
    {
        Table hz {};
        for (int n = 0; n < MasterScale::numMidiNotes; ++n)
            hz[static_cast<std::size_t> (n)] = refHz * std::exp2 ((n - ref) * periodCents / edo / 1200.0);
        return hz;
    }
}

TEST_CASE ("Master: 19 EDO com ref 60 vira 19 linhas por oitava")
{
    const auto layout = MasterScale::build (edoTable (19), 60, 2.0, 69);

    CHECK (layout.refNote == 60);
    CHECK_THAT (layout.refHz, WithinAbs (261.6255653, 1e-9));
    CHECK_THAT (layout.periodCents, WithinAbs (1200.0, 1e-9));
    REQUIRE (layout.numLines == 19);

    for (int k = 0; k < 19; ++k)
        CHECK_THAT (layout.lineCents[static_cast<std::size_t> (k)], WithinAbs (1200.0 * k / 19.0, 1e-6));
}

TEST_CASE ("Master: sem tecla de referencia usa a Ref Note do Matiza")
{
    const auto layout = MasterScale::build (edoTable (12, 69, 440.0), -1, 2.0, 69);
    CHECK (layout.refNote == 69);
    CHECK_THAT (layout.refHz, WithinAbs (440.0, 1e-9));
    CHECK (layout.numLines == 12);
}

TEST_CASE ("Master: periodo que nao e a oitava (Bohlen-Pierce)")
{
    // 13 divisões iguais de 3:1 (1902 ¢).
    const double period = 1200.0 * std::log2 (3.0);
    const auto layout = MasterScale::build (edoTable (13, 60, 261.6255653, period), 60, 3.0, 69);

    CHECK_THAT (layout.periodCents, WithinAbs (period, 1e-9));
    REQUIRE (layout.numLines == 13);

    // No desenho, a volta (0 a 1200) cobre o período inteiro: a linha 13 cairia em 1200.
    CHECK_THAT (MasterScale::toDisplay (layout.lineCents[1], layout), WithinAbs (1200.0 / 13.0, 1e-6));
    CHECK_THAT (MasterScale::fromDisplay (1200.0, layout), WithinAbs (period, 1e-9));
}

TEST_CASE ("Master: notas acendem a linha (nota - step 0) mod linhas")
{
    const auto layout = MasterScale::build (edoTable (19), 60, 2.0, 69);

    CHECK (MasterScale::locate (60, layout).step == 0);
    CHECK (MasterScale::locate (65, layout).step == 5);
    CHECK (MasterScale::locate (79, layout).step == 0);  // uma oitava acima
    CHECK (MasterScale::locate (79, layout).octave == 1);
    CHECK (MasterScale::locate (59, layout).step == 18); // abaixo do step 0
    CHECK (MasterScale::locate (59, layout).octave == -1);
}

TEST_CASE ("Master: altura da nota dentro do periodo")
{
    const auto hz = edoTable (19);
    const auto layout = MasterScale::build (hz, 60, 2.0, 69);

    // MIDI 65 em 19 EDO: 5 passos de 63,16 ¢ acima do step 0 (exemplo do teste no Ableton).
    CHECK_THAT (MasterScale::noteCentsInPeriod (hz, 65, layout), WithinAbs (5 * 1200.0 / 19.0, 1e-6));
    CHECK_THAT (MasterScale::noteCentsInPeriod (hz, 79, layout), WithinAbs (0.0, 1e-6));
    CHECK_THAT (MasterScale::noteCentsInPeriod (hz, 59, layout), WithinAbs (18 * 1200.0 / 19.0, 1e-6));
}

TEST_CASE ("Master: tabela invalida nao quebra o grafico")
{
    Table hz {}; // tudo zero: vale como 12-TET com A = 440
    const auto layout = MasterScale::build (hz, 60, 0.0, 69); // período inválido = oitava

    CHECK_THAT (layout.periodCents, WithinAbs (1200.0, 1e-9));
    CHECK (layout.numLines == 12);
    CHECK (layout.numLines <= MasterScale::maxLines);
}

TEST_CASE ("Master: escala com mais notas que o maximo de linhas e cortada")
{
    // 100 notas por oitava: o gráfico mostra no máximo 72 linhas.
    const auto layout = MasterScale::build (edoTable (100, 0, 20.0), 0, 2.0, 69);
    CHECK (layout.numLines == MasterScale::maxLines);
}

TEST_CASE ("Master: step 0 perto do topo do teclado")
{
    // Ref 120 em 12 EDO: só existem 8 notas acima (120 a 127), e o gráfico mostra essas.
    const auto layout = MasterScale::build (edoTable (12, 120, 8372.0), 120, 2.0, 69);
    CHECK (layout.refNote == 120);
    CHECK (layout.numLines == 8);
}
