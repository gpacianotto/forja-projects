#pragma once

// Motor de afinação do Matiza: converte nota MIDI em frequência para qualquer EDO
// (divisão igual da oitava).
//
//     f(n) = baseHz * 2^((n - refNote + bend * bendSteps) / edo)
//
// C++ puro, sem JUCE: pode ser testado sozinho. Não aloca memória, então é seguro chamar
// na thread de áudio.
class TuningEngine
{
public:
    // Valores padrão = temperamento de 12 tons com A = 440 Hz (RF2 do Spec).
    struct Settings
    {
        int    edo       = 12;    // divisões por oitava (>= 1)
        double baseHz    = 440.0; // frequência em que soa a nota de referência
        int    refNote   = 69;    // nota MIDI que soa em baseHz
        double bendSteps = 2.0;   // alcance do pitch bend, em passos da EDO
    };

    void setSettings (const Settings& newSettings) noexcept;
    const Settings& getSettings() const noexcept { return settings; }

    // bend vai de -1 (pitch wheel todo para baixo) a +1 (todo para cima); 0 = sem bend.
    // A nota é double para aceitar alturas "entre notas" (ex.: 60,5 no meio de um glide).
    double noteToHz (double midiNote, double bend = 0.0) const noexcept;

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
};
