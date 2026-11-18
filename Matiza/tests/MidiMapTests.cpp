// Testes do mapeamento do MIDI Learn (RF23).
// Nomes de TEST_CASE sem acento: o ctest no Windows não os encontra se tiverem.
#include "MidiMap.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE ("MidiMap: comeca vazio")
{
    MidiMap map;
    CHECK (map.isEmpty());
    CHECK (map.ccFor ("cutoff") == -1);
    CHECK (map.parameterFor (74).empty());
    CHECK (map.toString().empty());
}

TEST_CASE ("MidiMap: aprender liga o CC ao parametro nos dois sentidos")
{
    MidiMap map;
    map.assign (74, "cutoff");
    CHECK (map.ccFor ("cutoff") == 74);
    CHECK (map.parameterFor (74) == "cutoff");
    CHECK_FALSE (map.isEmpty());
}

TEST_CASE ("MidiMap: reaprender um knob troca o CC dele")
{
    MidiMap map;
    map.assign (74, "cutoff");
    map.assign (71, "cutoff");
    CHECK (map.ccFor ("cutoff") == 71);
    CHECK (map.parameterFor (74).empty());
}

TEST_CASE ("MidiMap: um CC que ja girava outro knob passa para o novo")
{
    MidiMap map;
    map.assign (74, "cutoff");
    map.assign (74, "reso");
    CHECK (map.parameterFor (74) == "reso");
    CHECK (map.ccFor ("cutoff") == -1);
}

TEST_CASE ("MidiMap: remover solta o knob")
{
    MidiMap map;
    map.assign (74, "cutoff");
    map.assign (1, "osc1Level");
    map.removeParameter ("cutoff");
    CHECK (map.ccFor ("cutoff") == -1);
    CHECK (map.ccFor ("osc1Level") == 1);
}

TEST_CASE ("MidiMap: CCs de modo do canal (120 a 127) e invalidos nao entram")
{
    MidiMap map;
    for (int cc : { 120, 123, 127, 128, -1 })
        map.assign (cc, "cutoff");
    map.assign (10, "");

    CHECK (map.isEmpty());
    CHECK (MidiMap::isLearnable (0));
    CHECK (MidiMap::isLearnable (119));
    CHECK_FALSE (MidiMap::isLearnable (120));
    CHECK (map.parameterFor (200).empty());
}

TEST_CASE ("MidiMap: texto do arquivo vai e volta igual")
{
    MidiMap map;
    map.assign (74, "cutoff");
    map.assign (1, "harmonicAlign");
    map.assign (21, "osc2Detune");

    CHECK (map.toString() == "1 harmonicAlign\n21 osc2Detune\n74 cutoff\n");
    CHECK (MidiMap::fromString (map.toString()) == map);
}

TEST_CASE ("MidiMap: arquivo estragado nao quebra, so ignora as linhas ruins")
{
    const auto map = MidiMap::fromString ("74 cutoff\r\n"
                                          "lixo\n"
                                          "200 reso\n"
                                          "121 gain\n"
                                          "12\n"
                                          "13 a b\n"
                                          "\n"
                                          "71 reso\n"
                                          "72 reso\n"); // repetido: fica o último, como no assign

    CHECK (map.ccFor ("cutoff") == 74);
    CHECK (map.ccFor ("reso") == 72);
    CHECK (map.ccFor ("gain") == -1);
    CHECK (map.parameterFor (12).empty());
    CHECK (map.parameterFor (13).empty());
}
