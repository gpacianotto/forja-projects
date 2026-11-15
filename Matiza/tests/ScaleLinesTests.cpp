// Testes das linhas da escala no gráfico do unison (RF19) e da leitura dos valores modulados
// para a modulação animada (RF20).
// Nomes de TEST_CASE sem acento: o ctest no Windows não os encontra se tiverem.
#include "ModulationDisplay.h"
#include "ScaleLines.h"
#include "TuningTable.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <limits>

using Catch::Matchers::WithinAbs;

namespace
{
    ScaleLines::Lines equalLines (int edo, double range = 100.0)
    {
        const TuningTable none; // sem afinação desenhada: afinação igual
        return ScaleLines::around ([&] (int k) { return none.stepsToCents (k, edo); }, range);
    }

    bool contains (const ScaleLines::Lines& lines, double cents)
    {
        return std::any_of (lines.cents.begin(), lines.cents.begin() + lines.count,
                            [cents] (double c) { return std::abs (c - cents) < 1e-6; });
    }
}

TEST_CASE ("Linhas da escala: 12 EDO tem os semitons nas pontas do eixo")
{
    // ±100 ¢ inclusive: os steps ±1 caem exatamente nas pontas.
    const auto lines = equalLines (12);
    CHECK (lines.count == 2);
    CHECK (contains (lines, 100.0));
    CHECK (contains (lines, -100.0));
}

TEST_CASE ("Linhas da escala: 19 EDO mostra os steps +-1 (63 cents)")
{
    const auto lines = equalLines (19);
    CHECK (lines.count == 2);
    CHECK (contains (lines, 1200.0 / 19.0));
    CHECK (contains (lines, -1200.0 / 19.0));
}

TEST_CASE ("Linhas da escala: 72 EDO tem 6 linhas de cada lado")
{
    const auto lines = equalLines (72);
    CHECK (lines.count == 12);
    CHECK (contains (lines, 100.0));
    CHECK (contains (lines, -1200.0 / 72.0));
}

TEST_CASE ("Linhas da escala: afinacao desenhada (RF17)")
{
    // 7 EDO com o step 1 puxado para 90 ¢ (o igual seria 171 ¢, fora do eixo).
    TuningTable table = TuningTable::equal (7);
    table.moveStep (1, 90.0, 7);

    const auto lines = ScaleLines::around ([&] (int k) { return table.stepsToCents (k, 7); }, 100.0);
    CHECK (contains (lines, 90.0));
    CHECK (lines.count == 1); // step -1 fica em 1029 - 1200 = -171 ¢, fora do eixo
}

TEST_CASE ("Linhas da escala: fim do teclado (NaN) para a busca")
{
    const auto lines = ScaleLines::around ([] (int k)
    {
        return k > 0 ? std::numeric_limits<double>::quiet_NaN() : 50.0 * k;
    }, 100.0);

    CHECK (lines.count == 2); // -50 e -100; para cima nada
    CHECK (contains (lines, -50.0));
    CHECK (contains (lines, -100.0));
}

TEST_CASE ("Modulacao animada: sem voz tocando nao ha valor")
{
    ModulationDisplay display;
    CHECK_FALSE (display.read (ModMatrix::Dest::cutoff).has_value());

    display.offsets[ModMatrix::index (ModMatrix::Dest::cutoff)].store (0.25f);
    display.active.store (true);
    REQUIRE (display.read (ModMatrix::Dest::cutoff).has_value());
    CHECK_THAT (*display.read (ModMatrix::Dest::cutoff), WithinAbs (0.25, 1e-6));
    CHECK_THAT (*display.read (ModMatrix::Dest::resonance), WithinAbs (0.0, 1e-6));

    display.active.store (false); // a voz acabou (fim do release): o ponto some
    CHECK_FALSE (display.read (ModMatrix::Dest::cutoff).has_value());
}
