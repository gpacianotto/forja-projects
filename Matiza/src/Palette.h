#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Paleta "Têmpera" da Matiza (Forja Instruments): as cores que o aço ganha ao ser aquecido na
// forja (palha, bronze, roxo, azul) sobre um fundo de ferro escuro. Toda cor da interface vem
// daqui; para mudar o visual, mude só este arquivo.
//
// Em Java, seria uma classe só de constantes (public static final Color ...). Em C++ usamos um
// namespace com variáveis `inline`, que podem ficar no header sem duplicar entre arquivos .cpp.
namespace Palette
{
    // Superfícies
    inline const juce::Colour background { 0xff14161b }; // fundo da janela
    inline const juce::Colour panel      { 0xff1f232b }; // grupos de controles, abas internas
    inline const juce::Colour widget     { 0xff353b46 }; // trilho dos knobs, fundo de caixas
    inline const juce::Colour outline    { 0xff4a515e }; // contornos

    // Texto
    inline const juce::Colour text      { 0xffe9e4da };
    inline const juce::Colour textMuted { 0xff8c8f98 };

    // Destaques
    inline const juce::Colour primary    { 0xffd9a441 }; // palha escura: valor dos knobs, curvas
    inline const juce::Colour secondary  { 0xffb8733a }; // bronze: ponteiro dos sliders
    inline const juce::Colour modulation { 0xff4f7fc4 }; // azul de têmpera: tudo que é modulação

    // Esquema de cores do LookAndFeel_V4 do JUCE (substitui o tema Midnight). A ordem dos
    // campos é a do construtor de juce::LookAndFeel_V4::ColourScheme.
    inline juce::LookAndFeel_V4::ColourScheme colourScheme()
    {
        return { background, // windowBackground
                 widget,     // widgetBackground
                 panel,      // menuBackground
                 outline,    // outline
                 text,       // defaultText
                 secondary,  // defaultFill
                 background, // highlightedText (texto sobre o destaque)
                 primary,    // highlightedFill
                 text };     // menuText
    }
}
