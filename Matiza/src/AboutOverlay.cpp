#include "AboutOverlay.h"

#include <BinaryData.h> // gerado pelo juce_add_binary_data (MatizaAssets) a partir de assets/

#include "Palette.h"

namespace
{
    constexpr int panelWidth  = 440;
    constexpr int panelHeight = 330;
    constexpr int padding     = 24;

    // Proporção da assinatura (viewBox 2000 × 780 do SVG).
    constexpr float signatureAspect = 2000.0f / 780.0f;

    const juce::String copyrightSign = juce::String (juce::CharPointer_UTF8 ("\xc2\xa9")); // ©
}

AboutOverlay::AboutOverlay()
{
    // SVG embutido no plugin (assets/matiza-assinatura-clara.svg). As cores do arquivo já são
    // as da marca (texto e cinza sobre fundo escuro); não precisa trocar.
    signature = juce::Drawable::createFromImageData (BinaryData::matizaassinaturaclara_svg,
                                                     BinaryData::matizaassinaturaclara_svgSize);

    closeButton.setColour (juce::TextButton::buttonColourId, Palette::widget);
    closeButton.setColour (juce::TextButton::textColourOffId, Palette::text);
    closeButton.onClick = [this] { hide(); };
    addAndMakeVisible (closeButton);

    setWantsKeyboardFocus (true);
    setVisible (false);
}

void AboutOverlay::show()
{
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
}

juce::Rectangle<int> AboutOverlay::panelBounds() const
{
    return getLocalBounds().withSizeKeepingCentre (juce::jmin (panelWidth, getWidth() - 32),
                                                   juce::jmin (panelHeight, getHeight() - 32));
}

void AboutOverlay::resized()
{
    auto panel = panelBounds().reduced (padding);
    closeButton.setBounds (panel.removeFromBottom (26).removeFromRight (80));
}

void AboutOverlay::paint (juce::Graphics& g)
{
    // Escurece o editor por trás, para o painel se destacar.
    g.fillAll (Palette::background.withAlpha (0.75f));

    const auto panelArea = panelBounds().toFloat();
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (panelArea, 8.0f);
    g.setColour (Palette::outline);
    g.drawRoundedRectangle (panelArea.reduced (0.5f), 8.0f, 1.0f);

    auto area = panelArea.reduced (static_cast<float> (padding));

    // Assinatura: largura do painel, mantendo a proporção (nunca esticar a logo).
    const float signatureHeight = juce::jmin (area.getWidth() / signatureAspect, 110.0f);
    if (signature != nullptr)
        signature->drawWithin (g, area.removeFromTop (signatureHeight).withSizeKeepingCentre (signatureHeight * signatureAspect, signatureHeight),
                               juce::RectanglePlacement::centred, 1.0f);

    area.removeFromTop (18.0f);

    auto line = [&g, &area] (const juce::String& text, juce::Colour colour, float size, bool bold = false)
    {
        g.setColour (colour);
        g.setFont (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
        g.drawText (text, area.removeFromTop (size + 6.0f), juce::Justification::centredLeft);
    };

    line ("Version " JucePlugin_VersionString, Palette::text, 14.0f, true);
    // Espaço do licenciado: o card de licenciamento troca este texto pelo nome do comprador.
    line ("License: not activated", Palette::textMuted, 13.0f);
    area.removeFromTop (10.0f);
    line ("Built with JUCE.", Palette::textMuted, 12.0f);
    line ("Logo lettering: Inter typeface, SIL Open Font License 1.1.", Palette::textMuted, 12.0f);
    line (copyrightSign + " 2026 Forja Instruments", Palette::textMuted, 12.0f);
}

void AboutOverlay::mouseDown (const juce::MouseEvent& event)
{
    // Clique fora do painel fecha; dentro do painel não faz nada.
    if (! panelBounds().contains (event.getPosition()))
        hide();
}

bool AboutOverlay::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        hide();
        return true;
    }
    return false;
}
