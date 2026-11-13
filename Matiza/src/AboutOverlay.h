#pragma once

#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

// Tela "Sobre" do Matiza: assinatura "MATIZA by Forja Instruments", versão, créditos e o espaço
// do nome do licenciado (preenchido quando o licenciamento existir).
//
// Não é uma janela do sistema: é um componente por cima do editor inteiro (janelas modais
// dentro de uma DAW costumam abrir atrás do host ou travar a interface). Fecha com clique fora
// do painel, com o botão Close ou com Esc.
class AboutOverlay final : public juce::Component
{
public:
    AboutOverlay();

    // Mostra por cima de tudo e pega o teclado (para o Esc funcionar).
    void show();
    void hide() { setVisible (false); }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    juce::Rectangle<int> panelBounds() const;

    std::unique_ptr<juce::Drawable> signature; // carregada uma vez, nunca no paint
    juce::TextButton closeButton { "Close" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutOverlay)
};
