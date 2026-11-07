#pragma once

// Motor de afinação do MicroSynth: converte nota MIDI em frequência para qualquer EDO
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
    double noteToHz (int midiNote, double bend = 0.0) const noexcept;

    // Converte o valor bruto do pitch wheel MIDI (0 a 16383, centro 8192) para -1..+1.
    static double pitchWheelToBend (int wheelValue) noexcept;

private:
    Settings settings;
};
