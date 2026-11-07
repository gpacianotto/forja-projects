#pragma once

// IDs dos parâmetros expostos ao host.
// ATENÇÃO: nunca mudar um ID depois de publicado — sets do Ableton salvos com o ID antigo
// perderiam o valor e a automação.
namespace ParamIDs
{
    // Afinação
    inline constexpr auto edo       = "edo";
    inline constexpr auto baseHz    = "baseHz";
    inline constexpr auto refNote   = "refNote";
    inline constexpr auto bendSteps = "bendSteps";

    // Oscilador
    inline constexpr auto wave = "wave";

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
}
