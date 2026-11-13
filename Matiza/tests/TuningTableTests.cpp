// Nomes de TEST_CASE sem acento: o ctest no Windows não os encontra se tiverem.
#include "TuningEngine.h"
#include "TuningTable.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace
{
    TuningEngine makeEngine (int edo, const TuningTable& table, double baseHz = 440.0, int refNote = 69)
    {
        TuningEngine engine;
        TuningEngine::Settings settings;
        settings.edo = edo;
        settings.baseHz = baseHz;
        settings.refNote = refNote;
        settings.table = table;
        engine.setSettings (settings);
        return engine;
    }
}

TEST_CASE ("Tabela igual soa exatamente como a afinacao sem tabela (RF17)")
{
    for (const int edo : { 1, 5, 12, 19, 31, 72 })
    {
        const auto withTable = makeEngine (edo, TuningTable::equal (edo));
        const auto without   = makeEngine (edo, TuningTable {});

        for (double note = 0.0; note <= 127.0; note += 0.25)       // inteiras e fracionárias (glide)
            for (const double bend : { -1.0, -0.3, 0.0, 0.7, 1.0 })
                CHECK_THAT (withTable.noteToHz (note, bend, 3.5), WithinRel (without.noteToHz (note, bend, 3.5), 1.0e-9));
    }
}

TEST_CASE ("Sem tabela, transposicao em steps = razao de transposeRatio (RF6)")
{
    const auto engine = makeEngine (19, TuningTable {});
    CHECK_THAT (engine.noteToHz (60, 0.0, 4.0),
                WithinRel (engine.noteToHz (60) * TuningEngine::transposeRatio (19, 0, 4.0, 0.0), 1.0e-12));
}

TEST_CASE ("Exemplo do Spec: 7 EDO, step 4 em 702 cents = 660 Hz")
{
    TuningTable table = TuningTable::equal (7);
    table.moveStep (4, 702.0, 7);

    const auto engine = makeEngine (7, table, 440.0, 69);
    CHECK_THAT (engine.noteToHz (73), WithinAbs (440.0 * std::exp2 (702.0 / 1200.0), 1.0e-9)); // ~660,8 Hz
    CHECK_THAT (engine.noteToHz (73), WithinAbs (660.0, 1.0));

    // Mesmo step uma oitava acima e uma abaixo: ×2 e ÷2.
    CHECK_THAT (engine.noteToHz (80), WithinRel (2.0 * engine.noteToHz (73), 1.0e-12));
    CHECK_THAT (engine.noteToHz (66), WithinRel (0.5 * engine.noteToHz (73), 1.0e-12));

    // Os outros steps continuam na posição igual.
    CHECK_THAT (engine.noteToHz (70), WithinRel (440.0 * std::exp2 (1.0 / 7.0), 1.0e-12));
}

TEST_CASE ("Tabela so vale para o EDO em que foi desenhada")
{
    TuningTable table = TuningTable::equal (7);
    table.moveStep (4, 702.0, 7);

    CHECK (table.isCustomFor (7));
    CHECK_FALSE (table.isCustomFor (8));

    // Com outro EDO, a mesma tabela dá a afinação igual.
    const auto engine = makeEngine (8, table);
    CHECK_THAT (engine.noteToHz (73), WithinRel (440.0 * std::exp2 (4.0 / 8.0), 1.0e-12));
}

TEST_CASE ("Altura fracionaria interpola em cents entre dois steps")
{
    TuningTable table = TuningTable::equal (7);
    table.moveStep (4, 702.0, 7);
    const auto engine = makeEngine (7, table);

    const double c3 = 1200.0 * 3.0 / 7.0;
    CHECK_THAT (engine.centsFromBase (72.5), WithinAbs ((c3 + 702.0) / 2.0, 1.0e-9));

    // Entre o último step e o 0 da oitava seguinte (1200 ¢).
    const double c6 = 1200.0 * 6.0 / 7.0;
    CHECK_THAT (engine.centsFromBase (75.5), WithinAbs ((c6 + 1200.0) / 2.0, 1.0e-9));

    // Abaixo da Ref Note: nota 68 = step 6 da oitava -1.
    CHECK_THAT (engine.centsFromBase (68), WithinAbs (c6 - 1200.0, 1.0e-9));

    // Pitch bend e transposição somam steps antes da tabela.
    CHECK_THAT (engine.centsFromBase (69, 0.0, 4.0), WithinAbs (702.0, 1.0e-9));
    CHECK_THAT (engine.centsFromBase (71, 1.0), WithinAbs (702.0, 1.0e-9)); // bend +2 steps
}

TEST_CASE ("moveStep nao deixa a linha cruzar as vizinhas nem mexe no step 0")
{
    TuningTable table = TuningTable::equal (4); // 0, 300, 600, 900

    CHECK_THAT (table.moveStep (2, 100.0, 4), WithinAbs (301.0, 1.0e-9));   // presa acima do step 1
    CHECK_THAT (table.moveStep (2, 2000.0, 4), WithinAbs (899.0, 1.0e-9));  // presa abaixo do step 3
    CHECK_THAT (table.moveStep (3, 1500.0, 4), WithinAbs (1199.0, 1.0e-9)); // último: abaixo da oitava
    CHECK_THAT (table.moveStep (1, -50.0, 4), WithinAbs (1.0, 1.0e-9));     // primeiro: acima do 0

    CHECK (table.moveStep (0, 500.0, 4) == 0.0);
    CHECK (table.cents[0] == 0.0);

    // Tabela de outro EDO: começa da igual do EDO atual.
    TuningTable other = TuningTable::equal (12);
    other.moveStep (1, 120.0, 5);
    CHECK (other.edo == 5);
    CHECK_THAT (other.cents[1], WithinAbs (120.0, 1.0e-9));
    CHECK_THAT (other.cents[2], WithinAbs (480.0, 1.0e-9));
}

TEST_CASE ("Tabela vai e volta pelo texto do estado")
{
    TuningTable table = TuningTable::equal (19);
    table.moveStep (5, 333.333, 19);
    table.moveStep (11, 702.0, 19);

    const auto restored = TuningTable::fromString (table.toString());
    REQUIRE (restored.has_value());
    CHECK (restored->edo == 19);
    for (int k = 0; k < 19; ++k)
        CHECK_THAT (restored->cents[static_cast<std::size_t> (k)], WithinAbs (table.cents[static_cast<std::size_t> (k)], 1.0e-6));

    CHECK (TuningTable::fromString ("1:").has_value()); // 1 EDO: só o step 0
}

TEST_CASE ("Texto invalido do estado e recusado")
{
    CHECK_FALSE (TuningTable::fromString ("").has_value());
    CHECK_FALSE (TuningTable::fromString ("abc").has_value());
    CHECK_FALSE (TuningTable::fromString ("0:").has_value());
    CHECK_FALSE (TuningTable::fromString ("73:1").has_value());
    CHECK_FALSE (TuningTable::fromString ("4:300,600").has_value());       // faltam valores
    CHECK_FALSE (TuningTable::fromString ("4:300,600,900,1000").has_value()); // sobram
    CHECK_FALSE (TuningTable::fromString ("4:600,300,900").has_value());   // fora de ordem
    CHECK_FALSE (TuningTable::fromString ("4:300,600,1250").has_value());  // passa da oitava
    CHECK_FALSE (TuningTable::fromString ("4:300,nan,900").has_value());
}
