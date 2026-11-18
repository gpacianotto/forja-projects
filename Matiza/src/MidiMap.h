#pragma once

#include <array>
#include <sstream>
#include <string>

// Mapeamento do MIDI Learn (RF23): qual CC gira qual knob. C++ puro, sem JUCE: testável.
//
// Regra: um CC por knob e um knob por CC. Os knobs são guardados pelo ID do parâmetro (o mesmo
// do host, estável para sempre), para o arquivo continuar valendo em versões futuras.
//
// Esta classe só guarda e transforma o mapeamento; quem o usa na thread de áudio é o MidiLearn,
// que copia a tabela para atômicos.
class MidiMap
{
public:
    static constexpr int numControllers = 128;

    // CCs 120 a 127 são mensagens de modo do canal (All Sound Off, All Notes Off...): não viram knob.
    static constexpr bool isLearnable (int cc) noexcept { return cc >= 0 && cc < 120; }

    // Liga o CC ao parâmetro. Se o parâmetro já tinha outro CC, o antigo é solto; se o CC já
    // girava outro parâmetro, passa a girar só este.
    void assign (int cc, const std::string& paramID)
    {
        if (! isLearnable (cc) || paramID.empty())
            return;

        removeParameter (paramID);
        params[static_cast<std::size_t> (cc)] = paramID;
    }

    void removeParameter (const std::string& paramID)
    {
        for (auto& p : params)
            if (p == paramID)
                p.clear();
    }

    // CC ligado ao parâmetro, ou -1.
    int ccFor (const std::string& paramID) const
    {
        for (int cc = 0; cc < numControllers; ++cc)
            if (! paramID.empty() && params[static_cast<std::size_t> (cc)] == paramID)
                return cc;
        return -1;
    }

    // Parâmetro ligado ao CC ("" = nenhum).
    const std::string& parameterFor (int cc) const
    {
        static const std::string none;
        return cc >= 0 && cc < numControllers ? params[static_cast<std::size_t> (cc)] : none;
    }

    bool isEmpty() const
    {
        for (const auto& p : params)
            if (! p.empty())
                return false;
        return true;
    }

    // Texto do arquivo: uma linha "CC ID" por mapeamento, em ordem de CC. Ex.: "74 cutoff".
    std::string toString() const
    {
        std::string out;
        for (int cc = 0; cc < numControllers; ++cc)
            if (! params[static_cast<std::size_t> (cc)].empty())
                out += std::to_string (cc) + " " + params[static_cast<std::size_t> (cc)] + "\n";
        return out;
    }

    // Lê o texto do arquivo. Linhas inválidas (CC fora da faixa, sem ID, lixo) são ignoradas,
    // para um arquivo estragado não impedir o plugin de abrir. Repetições seguem a regra do assign.
    static MidiMap fromString (const std::string& text)
    {
        MidiMap map;
        std::istringstream lines (text);
        std::string line;

        while (std::getline (lines, line))
        {
            std::istringstream fields (line);
            int cc = -1;
            std::string paramID, extra;

            if (fields >> cc >> paramID && ! (fields >> extra))
                map.assign (cc, paramID);
        }
        return map;
    }

    bool operator== (const MidiMap&) const = default;

private:
    std::array<std::string, numControllers> params {}; // params[cc] = ID do parâmetro ou ""
};
