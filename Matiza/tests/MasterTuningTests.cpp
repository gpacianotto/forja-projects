// Testes da afinação vinda do master do MTS-ESP (RF18), sem o MTS-ESP: a tabela é montada
// à mão, como o processador faria a cada bloco.
// Nomes de TEST_CASE sem acento: o ctest no Windows não os encontra se tiverem.
#include "TuningEngine.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <limits>

using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace
{
    // Master em 19 EDO com a nota 69 em 440 Hz (o que o MTS-ESP Mini entrega com um .scl de 19 EDO).
    TuningEngine::MasterTuning nineteenEdoMaster()
    {
        TuningEngine::MasterTuning master;
        master.active = true;
        for (int n = 0; n < TuningEngine::numMidiNotes; ++n)
            master.hz[static_cast<std::size_t> (n)] = 440.0 * std::exp2 ((n - 69) / 19.0);
        return master;
    }

    // Afinação interna bem diferente (12 EDO, 300 Hz, ref 60), para ficar óbvio que é ignorada.
    TuningEngine makeEngine (const TuningEngine::MasterTuning& master, double bendSteps = 2.0)
    {
        TuningEngine engine;
        engine.setSettings ({ 12, 300.0, 60, bendSteps });
        engine.setMasterTuning (master);
        return engine;
    }

    double hz19 (double note) { return 440.0 * std::exp2 ((note - 69.0) / 19.0); }
}

TEST_CASE ("MTS-ESP: sem master vale a afinacao interna")
{
    TuningEngine::MasterTuning master = nineteenEdoMaster();
    master.active = false; // Follow desligado ou nenhum master na sessão

    const auto engine = makeEngine (master);
    CHECK_FALSE (engine.followsMaster());
    CHECK_THAT (engine.noteToHz (60), WithinRel (300.0, 1e-12));
    CHECK_THAT (engine.noteToHz (72), WithinRel (600.0, 1e-12));
}

TEST_CASE ("MTS-ESP: com master cada nota toca na frequencia da tabela")
{
    const auto engine = makeEngine (nineteenEdoMaster());
    CHECK (engine.followsMaster());

    for (int note = 0; note < TuningEngine::numMidiNotes; ++note)
        CHECK_THAT (engine.noteToHz (note), WithinRel (hz19 (note), 1e-9));
}

TEST_CASE ("MTS-ESP: entre duas notas interpola em cents")
{
    const auto engine = makeEngine (nineteenEdoMaster());

    // Metade do caminho em cents = média geométrica das frequências.
    CHECK_THAT (engine.noteToHz (60.5), WithinRel (std::sqrt (hz19 (60) * hz19 (61)), 1e-9));
    CHECK_THAT (engine.noteToHz (60.25), WithinRel (hz19 (60.25), 1e-9));
}

TEST_CASE ("MTS-ESP: bend e transposicao andam em notas MIDI na tabela do master")
{
    const auto engine = makeEngine (nineteenEdoMaster(), 2.0);

    // Bend todo para cima com Bend Range = 2: duas notas acima.
    CHECK_THAT (engine.noteToHz (60, 1.0), WithinRel (hz19 (62), 1e-9));
    CHECK_THAT (engine.noteToHz (60, -0.5), WithinRel (hz19 (59), 1e-9));

    // Transposição de +4 steps (oscilador ou matriz): quatro notas acima.
    CHECK_THAT (engine.noteToHz (60, 0.0, 4.0), WithinRel (hz19 (64), 1e-9));
}

TEST_CASE ("MTS-ESP: fora de 0 a 127 continua com o intervalo da ponta")
{
    // Tabela irregular nas pontas, para conferir que o intervalo usado é o da ponta.
    TuningEngine::MasterTuning master = nineteenEdoMaster();
    master.hz[0] = 10.0;
    master.hz[1] = 15.0;   // 0 → 1: razão 1,5
    master.hz[126] = 8000.0;
    master.hz[127] = 10000.0; // 126 → 127: razão 1,25

    const auto engine = makeEngine (master);
    CHECK_THAT (engine.noteToHz (-1), WithinRel (10.0 / 1.5, 1e-9));
    CHECK_THAT (engine.noteToHz (-2), WithinRel (10.0 / 2.25, 1e-9));
    CHECK_THAT (engine.noteToHz (128), WithinRel (12500.0, 1e-9));
    CHECK_THAT (engine.noteToHz (127, 1.0), WithinRel (10000.0 * 1.25 * 1.25, 1e-9)); // bend +2
}

TEST_CASE ("MTS-ESP: notas filtradas pelo master")
{
    TuningEngine::MasterTuning master = nineteenEdoMaster();
    master.filtered[61] = true;

    const auto engine = makeEngine (master);
    CHECK (engine.isNoteFiltered (61));
    CHECK_FALSE (engine.isNoteFiltered (60));
    CHECK_FALSE (engine.isNoteFiltered (-1));
    CHECK_FALSE (engine.isNoteFiltered (128));

    // Sem master, nada é filtrado (a tabela antiga não vale).
    master.active = false;
    CHECK_FALSE (makeEngine (master).isNoteFiltered (61));
}

TEST_CASE ("MTS-ESP: frequencia invalida do master vira 12-TET com A = 440")
{
    TuningEngine::MasterTuning master = nineteenEdoMaster();
    master.hz[69] = 0.0;
    master.hz[70] = -5.0;
    master.hz[71] = std::numeric_limits<double>::quiet_NaN();

    const auto engine = makeEngine (master);
    CHECK_THAT (engine.noteToHz (69), WithinRel (440.0, 1e-9));
    CHECK_THAT (engine.noteToHz (70), WithinRel (440.0 * std::exp2 (1.0 / 12.0), 1e-9));
    CHECK_THAT (engine.noteToHz (71), WithinRel (440.0 * std::exp2 (2.0 / 12.0), 1e-9));
}

TEST_CASE ("MTS-ESP: centsFromBase conversa com noteToHz")
{
    // O ponto do gráfico dos steps usa centsFromBase; tem que mostrar a altura que soa.
    const auto engine = makeEngine (nineteenEdoMaster());
    for (double note : { 0.0, 33.3, 60.0, 69.0, 100.5, 127.0 })
    {
        const double fromCents = 300.0 * std::exp2 (engine.centsFromBase (note) / 1200.0);
        CHECK_THAT (fromCents, WithinRel (engine.noteToHz (note), 1e-9));
    }
}

TEST_CASE ("MTS-ESP: master sai da sessao e a afinacao interna volta")
{
    auto engine = makeEngine (nineteenEdoMaster());
    CHECK_THAT (engine.noteToHz (72), WithinRel (hz19 (72), 1e-9));

    TuningEngine::MasterTuning gone;
    engine.setMasterTuning (gone);
    CHECK_FALSE (engine.followsMaster());
    CHECK_THAT (engine.noteToHz (72), WithinRel (600.0, 1e-12));
}
