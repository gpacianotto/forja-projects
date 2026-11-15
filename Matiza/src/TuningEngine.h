#pragma once

#include <array>

#include "TuningTable.h"

// Motor de afinação do Matiza: converte nota MIDI em frequência para qualquer EDO
// (divisão igual da oitava).
//
//     f(n) = baseHz * 2^((n - refNote + bend * bendSteps) / edo)
//
// Com afinação desenhada (RF17), a altura em steps passa pela tabela antes:
//
//     f = baseHz * 2^((oitava * 1200 + cents[step]) / 1200)
//
// Com o master do MTS-ESP ativo (RF18), a afinação interna é ignorada: cada nota MIDI toca na
// frequência que o master define, e "1 step" = 1 nota MIDI (bend e transposição andam na
// tabela do master).
//
// C++ puro, sem JUCE e sem a biblioteca do MTS-ESP: pode ser testado sozinho. Quem fala com o
// MTS-ESP é o processador, que entrega a tabela pronta (setMasterTuning). Não aloca memória,
// então é seguro chamar na thread de áudio.
class TuningEngine
{
public:
    static constexpr int numMidiNotes = 128;

    // Tabela do master do MTS-ESP (RF18), copiada pelo processador a cada bloco.
    struct MasterTuning
    {
        bool active = false; // false = vale a afinação interna (Follow desligado ou sem master)
        std::array<double, numMidiNotes> hz {};      // frequência de cada nota MIDI
        std::array<bool, numMidiNotes>   filtered {}; // notas que o master manda não tocar
    };

    // Valores padrão = temperamento de 12 tons com A = 440 Hz (RF2 do Spec).
    struct Settings
    {
        int    edo       = 12;    // divisões por oitava (>= 1)
        double baseHz    = 440.0; // frequência em que soa a nota de referência
        int    refNote   = 69;    // nota MIDI que soa em baseHz
        double bendSteps = 2.0;   // alcance do pitch bend, em passos da EDO
        TuningTable table;        // afinação desenhada (RF17); só vale se table.edo == edo
    };

    void setSettings (const Settings& newSettings) noexcept;
    const Settings& getSettings() const noexcept { return settings; }

    // RF18. Frequência inválida (zero, negativa, NaN) vale como 12-TET com A = 440 Hz.
    void setMasterTuning (const MasterTuning& master) noexcept;
    bool followsMaster() const noexcept { return masterActive; }

    // O master manda não tocar esta nota? (sempre false sem master)
    bool isNoteFiltered (int midiNote) const noexcept;

    // bend vai de -1 (pitch wheel todo para baixo) a +1 (todo para cima); 0 = sem bend.
    // A nota é double para aceitar alturas "entre notas" (ex.: 60,5 no meio de um glide).
    // extraSteps: transposição em steps somada à nota antes da tabela (oscilador, matriz; RF17).
    double noteToHz (double midiNote, double bend = 0.0, double extraSteps = 0.0) const noexcept;

    // A mesma altura em cents a partir da Base Freq (para o ponto do gráfico dos steps, RF16).
    double centsFromBase (double midiNote, double bend = 0.0, double extraSteps = 0.0) const noexcept;

    // Razão de frequência de um oscilador transposto (RF6): 2^(oitavas + passos/edo + cents/1200).
    // Ex.: 1 oitava acima = 2,0; 0 de tudo = 1,0.
    static double transposeRatio (int edo, int octaves, double steps, double cents) noexcept;

    // Converte o valor bruto do pitch wheel MIDI (0 a 16383, centro 8192) para -1..+1.
    static double pitchWheelToBend (int wheelValue) noexcept;

    // RF4: só tocamos frequências entre 20 Hz e 45% da taxa de amostragem.
    // Abaixo é inaudível; acima geraria aliasing.
    static constexpr double minAudibleHz      = 20.0;
    static constexpr double maxNyquistFraction = 0.45;
    static bool isPlayable (double hz, double sampleRate) noexcept;

private:
    Settings settings;

    // Tabela do master guardada em cents absolutos (1200 × log2(Hz), a partir de 1 Hz): interpolar
    // em cents = interpolar na escala que o ouvido percebe. É calculada uma vez por bloco, não
    // a cada nota.
    bool masterActive = false;
    std::array<double, numMidiNotes> masterCents {};
    std::array<bool, numMidiNotes>   masterFiltered {};

    // Altura em cents absolutos de uma posição em notas MIDI (fracionária, pode sair de 0..127).
    double masterAbsoluteCents (double note) const noexcept;
    double masterNoteCents (int note) const noexcept;
};
