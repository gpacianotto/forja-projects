#pragma once

#include <array>
#include <cstddef>

// IDs dos parâmetros expostos ao host.
// ATENÇÃO: nunca mudar um ID depois de publicado — sets do Ableton salvos com o ID antigo
// perderiam o valor e a automação.
namespace ParamIDs
{
    // Afinação
    inline constexpr auto edo        = "edo";
    inline constexpr auto baseHz     = "baseHz";
    inline constexpr auto refNote    = "refNote";
    inline constexpr auto bendSteps  = "bendSteps";
    inline constexpr auto tuningLock = "tuningLock"; // trava da interface (RF8), não automatizável

    // Osciladores (RF6). Índice 0..2 = osc 1..3.
    // O osc 1 reaproveita o ID "wave" da v0.1, para sets antigos continuarem iguais.
    inline constexpr std::size_t numOscillators = 3;

    // std::array<const char*, 3>: array de tamanho fixo de strings C (como String[3] em Java).
    inline constexpr std::array<const char*, numOscillators> oscOn     { "osc1On",     "osc2On",     "osc3On" };
    inline constexpr std::array<const char*, numOscillators> oscWave   { "wave",       "osc2Wave",   "osc3Wave" };
    inline constexpr std::array<const char*, numOscillators> oscLevel  { "osc1Level",  "osc2Level",  "osc3Level" };
    inline constexpr std::array<const char*, numOscillators> oscOctave { "osc1Octave", "osc2Octave", "osc3Octave" };
    inline constexpr std::array<const char*, numOscillators> oscSteps  { "osc1Steps",  "osc2Steps",  "osc3Steps" };
    inline constexpr std::array<const char*, numOscillators> oscFine   { "osc1Fine",   "osc2Fine",   "osc3Fine" };

    // Vozes (RF7)
    inline constexpr auto voiceMode = "voiceMode"; // 0 = Poly, 1 = Mono
    inline constexpr auto legato    = "legato";
    inline constexpr auto glide     = "glide";     // ms

    // Envelope de amplitude (tempos em ms)
    inline constexpr auto attack  = "attack";
    inline constexpr auto decay   = "decay";
    inline constexpr auto sustain = "sustain";
    inline constexpr auto release = "release";

    // Filtro passa-baixa
    inline constexpr auto cutoff = "cutoff";
    inline constexpr auto reso   = "reso";

    // Saída
    inline constexpr auto gain = "gain";

    // Matriz de modulação (RF10): 8 slots. Índice 0..7 = slot 1..8.
    inline constexpr std::size_t numModSlots = 8;
    inline constexpr std::array<const char*, numModSlots> modSource { "mod1Source", "mod2Source", "mod3Source", "mod4Source", "mod5Source", "mod6Source", "mod7Source", "mod8Source" };
    inline constexpr std::array<const char*, numModSlots> modDest   { "mod1Dest", "mod2Dest", "mod3Dest", "mod4Dest", "mod5Dest", "mod6Dest", "mod7Dest", "mod8Dest" };
    inline constexpr std::array<const char*, numModSlots> modAmount { "mod1Amount", "mod2Amount", "mod3Amount", "mod4Amount", "mod5Amount", "mod6Amount", "mod7Amount", "mod8Amount" };

    // Envelope de modulação (Mod Env), tempos em ms
    inline constexpr auto modEnvAttack  = "modEnvAttack";
    inline constexpr auto modEnvDecay   = "modEnvDecay";
    inline constexpr auto modEnvSustain = "modEnvSustain";
    inline constexpr auto modEnvRelease = "modEnvRelease";
}
