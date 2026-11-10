#pragma once

#include <algorithm>
#include <array>
#include <cstddef>

// Matriz de modulação (RF10). C++ puro, sem JUCE: testável sozinha e segura na thread de áudio
// (só arrays de tamanho fixo, nada de alocação).
//
// Cada slot liga uma FONTE (ex.: velocity) a um DESTINO (ex.: cutoff) com uma QUANTIDADE
// (-1 a +1). A cada bloco, cada voz calcula quanto cada destino deve se deslocar:
//     deslocamento[destino] = soma de (valor da fonte × quantidade) dos slots daquele destino
namespace ModMatrix
{
    inline constexpr std::size_t numSlots = 8;

    // ATENÇÃO: a ordem é o valor salvo nos sets do Ableton. Nunca reordenar nem remover;
    // fontes novas só ocupam as vagas reservadas (reserved1, reserved2).
    enum class Source
    {
        none = 0,
        velocity,
        modWheel,
        aftertouch,
        keyTrack,
        modEnv,
        lfo1,
        lfo2,
        lfo3,
        reserved1,
        reserved2,
        count // número de opções (não é uma fonte)
    };

    // Mesma regra: ordem fixa para sempre; destinos novos só nas vagas reservadas.
    enum class Dest
    {
        none = 0,
        cutoff,
        resonance,
        pitch,      // todos os osciladores
        osc1Level, osc1Pitch, osc1Fine,
        osc2Level, osc2Pitch, osc2Fine,
        osc3Level, osc3Pitch, osc3Fine,
        reserved1, reserved2, reserved3, reserved4,
        count
    };

    inline constexpr std::size_t numSources = static_cast<std::size_t> (Source::count);
    inline constexpr std::size_t numDests   = static_cast<std::size_t> (Dest::count);

    // Nomes mostrados no host e na interface (mesma ordem dos enums).
    inline constexpr std::array<const char*, numSources> sourceNames {
        "None", "Velocity", "Mod Wheel", "Aftertouch", "Key Track", "Mod Env",
        "LFO 1", "LFO 2", "LFO 3", "Reserved 1", "Reserved 2"
    };

    inline constexpr std::array<const char*, numDests> destNames {
        "None", "Cutoff", "Resonance", "Pitch",
        "Osc 1 Level", "Osc 1 Pitch", "Osc 1 Fine",
        "Osc 2 Level", "Osc 2 Pitch", "Osc 2 Fine",
        "Osc 3 Level", "Osc 3 Pitch", "Osc 3 Fine",
        "Reserved 1", "Reserved 2", "Reserved 3", "Reserved 4"
    };

    // Quantas opções a interface mostra (as reservadas ficam escondidas; estão no fim).
    inline constexpr std::size_t numUsableSources = static_cast<std::size_t> (Source::reserved1);
    inline constexpr std::size_t numUsableDests   = static_cast<std::size_t> (Dest::reserved1);

    // No destino Pitch (e no Pitch de cada oscilador), quantidade 100 % = 24 passos da EDO.
    inline constexpr double pitchStepsAtFull = 24.0;

    // Destinos de cada oscilador (oscIndex 0..2).
    constexpr Dest oscLevelDest (std::size_t oscIndex) { return static_cast<Dest> (static_cast<int> (Dest::osc1Level) + 3 * static_cast<int> (oscIndex)); }
    constexpr Dest oscPitchDest (std::size_t oscIndex) { return static_cast<Dest> (static_cast<int> (Dest::osc1Pitch) + 3 * static_cast<int> (oscIndex)); }
    constexpr Dest oscFineDest  (std::size_t oscIndex) { return static_cast<Dest> (static_cast<int> (Dest::osc1Fine)  + 3 * static_cast<int> (oscIndex)); }

    // Fontes bipolares vão de -1 a +1 (oscilam em volta do valor do knob);
    // as unipolares vão de 0 a 1 (só empurram para um lado).
    constexpr bool isBipolar (Source source)
    {
        return source == Source::keyTrack
            || source == Source::lfo1 || source == Source::lfo2 || source == Source::lfo3;
    }

    // Key Track: posição da tecla MIDI, -1 na nota 0 e +1 na nota 127.
    constexpr float keyTrack (int midiNote)
    {
        return (static_cast<float> (std::clamp (midiNote, 0, 127)) - 63.5f) / 63.5f;
    }

    struct Slot
    {
        Source source = Source::none;
        Dest   dest   = Dest::none;
        float  amount = 0.0f; // -1..+1 (o parâmetro vai de -100 a +100 %)

        constexpr bool isActive() const { return source != Source::none && dest != Dest::none && amount != 0.0f; }
    };

    using Slots        = std::array<Slot, numSlots>;
    using SourceValues = std::array<float, numSources>; // valor atual de cada fonte
    using DestOffsets  = std::array<float, numDests>;   // deslocamento de cada destino

    constexpr std::size_t index (Source s) { return static_cast<std::size_t> (s); }
    constexpr std::size_t index (Dest d)   { return static_cast<std::size_t> (d); }

    // Soma a contribuição de todos os slots ativos. Vários slots no mesmo destino se somam.
    inline void computeOffsets (const Slots& slots, const SourceValues& sources, DestOffsets& offsets)
    {
        offsets.fill (0.0f);

        for (const auto& slot : slots)
            if (slot.isActive())
                offsets[index (slot.dest)] += sources[index (slot.source)] * slot.amount;
    }

    // Aplica um deslocamento a um valor normalizado (0 a 1), sem sair da faixa.
    constexpr float applyNormalized (float baseNormalized, float offset)
    {
        return std::clamp (baseNormalized + offset, 0.0f, 1.0f);
    }
}
