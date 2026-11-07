#pragma once

// IDs dos parâmetros expostos ao host.
// ATENÇÃO: nunca mudar um ID depois de publicado — sets do Ableton salvos com o ID antigo
// perderiam o valor e a automação.
namespace ParamIDs
{
    // Afinação
    inline constexpr auto edo     = "edo";
    inline constexpr auto baseHz  = "baseHz";
    inline constexpr auto refNote = "refNote";
}
