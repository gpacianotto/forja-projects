#include "Controls.h"
#include "ModulationUI.h"
#include "Palette.h"

namespace
{
    constexpr int labelHeight = 18;
    constexpr int titleHeight = 24;

    void setupLabel (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions (13.0f));
    }
}

// ----------------------------------------------------------------------------- KnobSlider

void KnobSlider::mouseDown (const juce::MouseEvent& e)
{
    popupGesture = e.mods.isPopupMenu() && onPopupMenu != nullptr;

    if (popupGesture)
        onPopupMenu();
    else
        juce::Slider::mouseDown (e);
}

void KnobSlider::mouseDrag (const juce::MouseEvent& e)
{
    if (! popupGesture)
        juce::Slider::mouseDrag (e);
}

void KnobSlider::mouseUp (const juce::MouseEvent& e)
{
    if (! popupGesture)
        juce::Slider::mouseUp (e);

    popupGesture = false;
}

// ----------------------------------------------------------------------------- Knob

Knob::Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& labelText)
    // O slider precisa existir antes do attachment: a lista de inicialização segue a ordem
    // de declaração no .h (label, slider, attachment).
    : attachment (apvts, paramID, slider)
{
    setupLabel (label, labelText);
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, labelHeight);

    if (auto* param = apvts.getParameter (paramID))
    {
        // Duplo clique volta ao valor padrão do parâmetro.
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));

        // Unidade do parâmetro (Hz, ms, dB...) depois do número.
        if (param->getLabel().isNotEmpty())
            slider.setTextValueSuffix (" " + param->getLabel());
    }

    addAndMakeVisible (slider);
}

Knob::~Knob() = default;

void Knob::enableModulation (ModulationModel& model, ModMatrix::Dest dest, float ringScale)
{
    modulationModel = &model;
    modDest = dest;
    ring = std::make_unique<ModRingOverlay> (model, dest, slider, ringScale);
    addAndMakeVisible (*ring); // por cima do slider: só captura o mouse sobre os anéis

    ring->onRingCountChanged = [this] { resized(); };
    ring->onPopupMenu        = [this] { showModulationMenu(); };
    slider.onPopupMenu       = [this] { showModulationMenu(); };
}

void Knob::showModulationMenu()
{
    if (modulationModel == nullptr)
        return;

    auto& model = *modulationModel;
    const auto slots = model.slotsFor (modDest);

    // IDs dos itens: slot × 10 + ação (0 não pode ser usado: é "menu fechado sem escolha").
    enum Action { invert = 1, zero = 2, remove = 3 };

    juce::PopupMenu menu;
    menu.addSectionHeader ("Modulation");

    if (slots.isEmpty())
        menu.addItem (juce::PopupMenu::Item ("Drag a source from the header onto this knob").setEnabled (false));

    for (int slotIndex : slots)
    {
        const auto slot = model.getSlot (static_cast<std::size_t> (slotIndex));
        const float percent = model.getAmountPercent (static_cast<std::size_t> (slotIndex));
        const int base = (slotIndex + 1) * 10;

        juce::PopupMenu actions;
        actions.addItem (base + invert, "Invert");
        actions.addItem (base + zero,   "Set amount to 0 %");
        actions.addSeparator();
        actions.addItem (base + remove, "Remove");

        juce::PopupMenu::Item item (juce::String (ModMatrix::sourceNames[ModMatrix::index (slot.source)])
                                    + ": " + (percent > 0.0f ? "+" : "") + juce::String (percent, 0) + " %");
        item.subMenu = std::make_unique<juce::PopupMenu> (std::move (actions));
        item.colour  = ModulationModel::sourceColour (slot.source);
        menu.addItem (std::move (item));
    }

    // O menu é assíncrono: o knob pode ser destruído (janela fechada) antes da escolha.
    // SafePointer vira nulo nesse caso, como uma WeakReference em Java.
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slider),
                        [safeThis = juce::Component::SafePointer<Knob> (this)] (int result)
                        {
                            if (safeThis == nullptr || safeThis->modulationModel == nullptr || result <= 0)
                                return;

                            auto& m = *safeThis->modulationModel;
                            const auto slotIndex = static_cast<std::size_t> (result / 10 - 1);
                            const int action = result % 10;

                            if (action == remove)
                            {
                                m.clearSlot (slotIndex);
                                return;
                            }

                            const float newPercent = action == invert ? -m.getAmountPercent (slotIndex) : 0.0f;
                            m.beginAmountGesture (slotIndex);
                            m.setAmountPercent (slotIndex, newPercent);
                            m.endAmountGesture (slotIndex);
                        });
}

void Knob::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (labelHeight));

    if (ring == nullptr)
    {
        slider.setBounds (area);
        return;
    }

    // RF14: com N anéis, o slider encolhe (N - 1) passos de cada lado para abrir espaço; os
    // anéis ficam na área original. Sem deixar o knob menor que ~40 px.
    const int extraRings = juce::jmax (0, ring->getNumRings() - 1);
    const int maxInset = juce::jmax (0, (juce::jmin (area.getWidth(), area.getHeight() - labelHeight) - 40) / 2);
    const int inset = juce::jmin (maxInset, juce::roundToInt (extraRings * ModRingOverlay::ringStep));

    ring->setBounds (area);
    slider.setBounds (area.reduced (inset));
}

void Knob::paintOverChildren (juce::Graphics& g)
{
    // Contorno enquanto uma fonte é arrastada por cima: "solte aqui".
    if (dropHighlight)
    {
        g.setColour (ModulationModel::accentColour());
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 6.0f, 2.0f);
    }
}

bool Knob::isInterestedInDragSource (const SourceDetails& details)
{
    return modulationModel != nullptr && ModulationModel::parseDragDescription (details.description) != ModMatrix::Source::none;
}

void Knob::itemDragEnter (const SourceDetails&)
{
    dropHighlight = true;
    repaint();
}

void Knob::itemDragExit (const SourceDetails&)
{
    dropHighlight = false;
    repaint();
}

void Knob::itemDropped (const SourceDetails& details)
{
    dropHighlight = false;
    repaint();

    const auto source = ModulationModel::parseDragDescription (details.description);
    if (modulationModel->addLink (source, modDest) < 0)
    {
        // Os 8 slots estão ocupados.
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::WarningIcon)
                                          .withTitle ("Modulation matrix full")
                                          .withMessage ("All 8 slots are in use. Delete a slot in the Matrix tab first.")
                                          .withButton ("OK")
                                          .withAssociatedComponent (this),
                                      nullptr);
    }
}

// ----------------------------------------------------------------------------- Choice

Choice::Choice (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& labelText)
{
    setupLabel (label, labelText);
    addAndMakeVisible (label);

    // As opções vêm do próprio parâmetro, para nunca ficarem diferentes do que o host vê.
    // Precisam estar na lista antes de criar o attachment.
    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramID)))
        comboBox.addItemList (choiceParam->choices, 1);

    comboBox.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (comboBox);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, paramID, comboBox);
}

void Choice::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (labelHeight));

    // Combo com altura fixa, centralizado verticalmente no espaço que sobra.
    const int comboHeight = 26;
    comboBox.setBounds (area.withSizeKeepingCentre (juce::jmin (area.getWidth() - 8, 140), comboHeight));
}

// ----------------------------------------------------------------------------- Toggle

Toggle::Toggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& labelText)
    : attachment (apvts, paramID, button)
{
    setupLabel (label, labelText);
    addAndMakeVisible (label);
    addAndMakeVisible (button);
}

void Toggle::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (labelHeight));

    // O ToggleButton desenha a caixinha encostada à esquerda (4 px de margem); um botão
    // estreito e centralizado deixa a caixinha no meio da coluna.
    button.setBounds (area.withSizeKeepingCentre (24, 28));
}

// ----------------------------------------------------------------------------- ParameterGroup

ParameterGroup::ParameterGroup (const juce::String& groupTitle) : title (groupTitle) {}

void ParameterGroup::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const auto& lf = getLookAndFeel();

    g.setColour (Palette::panel);
    g.fillRoundedRectangle (bounds, 8.0f);

    g.setColour (Palette::outline);
    g.drawRoundedRectangle (bounds, 8.0f, 1.0f);

    g.setColour (lf.findColour (juce::Label::textColourId));
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    // drawFittedText comprime o texto (até 70% da largura) em vez de cortá-lo com "..."
    // quando a janela está estreita.
    g.drawFittedText (title.toUpperCase(), getLocalBounds().removeFromTop (titleHeight).reduced (8, 0),
                      juce::Justification::centredLeft, 1, 0.7f);
}

void ParameterGroup::resized()
{
    auto area = getLocalBounds().reduced (6);
    area.removeFromTop (titleHeight - 6);

    // Com gráfico: ele fica com a parte de cima (ou da esquerda), e os controles com o resto.
    if (display != nullptr)
    {
        if (displayPlacement == DisplayPlacement::left)
        {
            display->setBounds (area.removeFromLeft (juce::roundToInt (area.getWidth() * displayFraction)).reduced (2, 0));
            area.removeFromLeft (8);
        }
        else
        {
            // Os controles nunca ficam com menos de ~100 px: abaixo disso, o nome e a caixa de
            // valor ocupam quase tudo e o knob some.
            constexpr int minControlsHeight = 100;
            const int displayHeight = juce::jmin (juce::roundToInt (area.getHeight() * displayFraction),
                                                  area.getHeight() - minControlsHeight);
            display->setBounds (area.removeFromTop (juce::jmax (0, displayHeight)).reduced (2, 0));
            area.removeFromTop (4);
        }
    }

    // FlexBox: como o flexbox do CSS. Cada controle ocupa uma fatia igual da largura.
    juce::FlexBox row;
    row.flexDirection = juce::FlexBox::Direction::row;

    for (auto& control : controls)
        row.items.add (juce::FlexItem (*control).withFlex (1.0f).withMargin (2.0f));

    row.performLayout (area);
}
