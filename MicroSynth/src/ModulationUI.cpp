#include "ModulationUI.h"

#include <cmath>

using ModMatrix::Dest;
using ModMatrix::Source;

// ----------------------------------------------------------------------------- ModulationModel

ModulationModel::ModulationModel (juce::AudioProcessorValueTreeState& apvts)
{
    for (std::size_t i = 0; i < ParamIDs::numModSlots; ++i)
    {
        sourceParams[i] = apvts.getParameter (ParamIDs::modSource[i]);
        destParams[i]   = apvts.getParameter (ParamIDs::modDest[i]);
        amountParams[i] = apvts.getParameter (ParamIDs::modAmount[i]);
        jassert (sourceParams[i] != nullptr && destParams[i] != nullptr && amountParams[i] != nullptr);
    }
}

void ModulationModel::setParameter (juce::RangedAudioParameter& param, float value)
{
    param.setValueNotifyingHost (param.convertTo0to1 (value));
}

ModMatrix::Slot ModulationModel::getSlot (std::size_t i) const
{
    ModMatrix::Slot slot;
    slot.source = static_cast<Source> (juce::roundToInt (sourceParams[i]->convertFrom0to1 (sourceParams[i]->getValue())));
    slot.dest   = static_cast<Dest>   (juce::roundToInt (destParams[i]->convertFrom0to1 (destParams[i]->getValue())));
    slot.amount = getAmountPercent (i) / 100.0f;
    return slot;
}

int ModulationModel::addLink (Source source, Dest dest)
{
    // Ligação repetida: reaproveita o slot existente.
    for (std::size_t i = 0; i < ParamIDs::numModSlots; ++i)
    {
        const auto slot = getSlot (i);
        if (slot.source == source && slot.dest == dest)
            return static_cast<int> (i);
    }

    // Primeiro slot livre (sem fonte ou sem destino).
    for (std::size_t i = 0; i < ParamIDs::numModSlots; ++i)
    {
        const auto slot = getSlot (i);
        if (slot.source == Source::none || slot.dest == Dest::none)
        {
            setParameter (*sourceParams[i], static_cast<float> (source));
            setParameter (*destParams[i],   static_cast<float> (dest));

            amountParams[i]->beginChangeGesture();
            setParameter (*amountParams[i], 50.0f);
            amountParams[i]->endChangeGesture();
            return static_cast<int> (i);
        }
    }

    return -1;
}

void ModulationModel::clearSlot (std::size_t i)
{
    setParameter (*sourceParams[i], 0.0f);
    setParameter (*destParams[i],   0.0f);

    amountParams[i]->beginChangeGesture();
    setParameter (*amountParams[i], 0.0f);
    amountParams[i]->endChangeGesture();
}

int ModulationModel::firstSlotFor (Dest dest) const
{
    for (std::size_t i = 0; i < ParamIDs::numModSlots; ++i)
    {
        const auto slot = getSlot (i);
        if (slot.dest == dest && slot.source != Source::none)
            return static_cast<int> (i);
    }

    return -1;
}

float ModulationModel::getAmountPercent (std::size_t i) const
{
    return amountParams[i]->convertFrom0to1 (amountParams[i]->getValue());
}

void ModulationModel::beginAmountGesture (std::size_t i) { amountParams[i]->beginChangeGesture(); }
void ModulationModel::endAmountGesture (std::size_t i)   { amountParams[i]->endChangeGesture(); }

void ModulationModel::setAmountPercent (std::size_t i, float percent)
{
    setParameter (*amountParams[i], juce::jlimit (-100.0f, 100.0f, percent));
}

ModulationModel::Range ModulationModel::getRange (Dest dest) const
{
    Range range;

    for (std::size_t i = 0; i < ParamIDs::numModSlots; ++i)
    {
        const auto slot = getSlot (i);
        if (slot.dest != dest || slot.source == Source::none)
            continue;

        range.active = true; // mesmo com quantidade 0 o anel aparece, para poder ser arrastado

        if (ModMatrix::isBipolar (slot.source))
        {
            range.low  -= std::abs (slot.amount);
            range.high += std::abs (slot.amount);
        }
        else
        {
            range.low  += juce::jmin (0.0f, slot.amount);
            range.high += juce::jmax (0.0f, slot.amount);
        }
    }

    return range;
}

juce::var ModulationModel::makeDragDescription (Source source)
{
    return "modSource:" + juce::String (static_cast<int> (source));
}

Source ModulationModel::parseDragDescription (const juce::var& description)
{
    const auto text = description.toString();
    if (! text.startsWith ("modSource:"))
        return Source::none;

    const int value = text.fromFirstOccurrenceOf (":", false, false).getIntValue();
    if (value <= 0 || value >= static_cast<int> (ModMatrix::numUsableSources))
        return Source::none;

    return static_cast<Source> (value);
}

// ----------------------------------------------------------------------------- ModRingOverlay

namespace
{
    constexpr float ringThickness = 3.0f;
    constexpr float ringGap       = 5.0f;  // distância entre o arco do knob e o anel
    constexpr float ringHitSlop   = 5.0f;  // tolerância do clique em volta do anel
}

ModRingOverlay::ModRingOverlay (ModulationModel& m, Dest d, juce::Slider& s, float ringScale)
    : model (m), dest (d), slider (s), scale (ringScale)
{
    startTimerHz (30);
}

ModRingOverlay::Geometry ModRingOverlay::getGeometry() const
{
    // Mesma conta do LookAndFeel_V4::drawRotarySlider, para o anel ficar concêntrico ao knob.
    auto sliderBounds = getLocalBounds();
    if (auto* methods = dynamic_cast<juce::Slider::LookAndFeelMethods*> (&slider.getLookAndFeel()))
        sliderBounds = methods->getSliderLayout (slider).sliderBounds;

    const auto bounds = sliderBounds.toFloat().reduced (10.0f);
    const auto rotary = slider.getRotaryParameters();

    Geometry geometry;
    geometry.centre     = bounds.getCentre();
    geometry.radius     = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f + ringGap;
    geometry.startAngle = rotary.startAngleRadians;
    geometry.endAngle   = rotary.endAngleRadians;
    return geometry;
}

void ModRingOverlay::paint (juce::Graphics& g)
{
    drawnProportion = slider.valueToProportionOfLength (slider.getValue());
    drawnRange      = model.getRange (dest);

    if (! drawnRange.active)
        return;

    const auto geo = getGeometry();
    if (geo.radius <= ringGap)
        return;

    auto angleFor = [&geo] (double proportion)
    {
        return geo.startAngle + static_cast<float> (juce::jlimit (0.0, 1.0, proportion)) * (geo.endAngle - geo.startAngle);
    };

    // Trilho fraco: mostra que o knob é modulado e onde dá para pegar o anel.
    juce::Path track;
    track.addCentredArc (geo.centre.x, geo.centre.y, geo.radius, geo.radius, 0.0f, geo.startAngle, geo.endAngle, true);
    g.setColour (ModulationModel::accentColour().withAlpha (0.2f));
    g.strokePath (track, juce::PathStrokeType (ringThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Alcance da modulação, a partir da posição atual do knob.
    const double from = drawnProportion + drawnRange.low * scale;
    const double to   = drawnProportion + drawnRange.high * scale;

    juce::Path arc;
    arc.addCentredArc (geo.centre.x, geo.centre.y, geo.radius, geo.radius, 0.0f, angleFor (from), angleFor (to), true);
    g.setColour (ModulationModel::accentColour());
    g.strokePath (arc, juce::PathStrokeType (ringThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

bool ModRingOverlay::hitTest (int x, int y)
{
    if (! model.getRange (dest).active)
        return false; // sem modulação: o mouse vai direto para o slider

    const auto geo = getGeometry();
    const float distance = geo.centre.getDistanceFrom ({ static_cast<float> (x), static_cast<float> (y) });
    return std::abs (distance - geo.radius) <= ringHitSlop;
}

void ModRingOverlay::mouseDown (const juce::MouseEvent&)
{
    draggingSlot = model.firstSlotFor (dest);
    if (draggingSlot < 0)
        return;

    dragStartAmount = model.getAmountPercent (static_cast<std::size_t> (draggingSlot));
    model.beginAmountGesture (static_cast<std::size_t> (draggingSlot));
}

void ModRingOverlay::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingSlot < 0)
        return;

    // Para cima aumenta; 150 px = 100 %. Com Shift, ajuste fino (10x mais devagar).
    const float pixelsPerFullRange = e.mods.isShiftDown() ? 1500.0f : 150.0f;
    const float delta = -static_cast<float> (e.getDistanceFromDragStartY()) / pixelsPerFullRange * 100.0f;
    model.setAmountPercent (static_cast<std::size_t> (draggingSlot), dragStartAmount + delta);
    repaint();
}

void ModRingOverlay::mouseUp (const juce::MouseEvent&)
{
    if (draggingSlot >= 0)
        model.endAmountGesture (static_cast<std::size_t> (draggingSlot));

    draggingSlot = -1;
}

void ModRingOverlay::timerCallback()
{
    const auto range = model.getRange (dest);
    const double proportion = slider.valueToProportionOfLength (slider.getValue());

    if (proportion != drawnProportion || range.active != drawnRange.active
        || range.low != drawnRange.low || range.high != drawnRange.high)
        repaint();
}

// ----------------------------------------------------------------------------- SourceChip

SourceChip::SourceChip (Source s) : source (s)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setTooltip ("Drag onto a knob to modulate it");
}

void SourceChip::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const auto colour = ModulationModel::accentColour();

    g.setColour (colour.withAlpha (0.15f));
    g.fillRoundedRectangle (bounds, bounds.getHeight() / 2.0f);
    g.setColour (colour);
    g.drawRoundedRectangle (bounds, bounds.getHeight() / 2.0f, 1.0f);

    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawFittedText (ModMatrix::sourceNames[ModMatrix::index (source)], getLocalBounds().reduced (6, 0),
                      juce::Justification::centred, 1, 0.7f);
}

void SourceChip::mouseDrag (const juce::MouseEvent&)
{
    // O editor é o DragAndDropContainer: ele desenha a alça seguindo o mouse e entrega o
    // texto da fonte ao knob onde ela for solta.
    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
        if (! container->isDragAndDropActive())
            container->startDragging (ModulationModel::makeDragDescription (source), this);
}

// ----------------------------------------------------------------------------- ModSlotList

namespace
{
    // Liga um ComboBox a um parâmetro de escolha pelo ID do item (ID = índice + 1).
    //
    // O ComboBoxAttachment do JUCE calcula o índice a partir do NÚMERO de itens do combo,
    // então só funciona se o combo tiver todas as opções. Aqui as opções reservadas ficam
    // escondidas, por isso a ligação é feita à mão com um ParameterAttachment.
    class ChoiceIdAttachment
    {
    public:
        ChoiceIdAttachment (juce::RangedAudioParameter& param, juce::ComboBox& box)
            : comboBox (box),
              attachment (param, [this] (float value) { comboBox.setSelectedId (juce::roundToInt (value) + 1, juce::dontSendNotification); })
        {
            comboBox.onChange = [this]
            {
                if (comboBox.getSelectedId() > 0)
                    attachment.setValueAsCompleteGesture (static_cast<float> (comboBox.getSelectedId() - 1));
            };
            attachment.sendInitialUpdate();
        }

        ~ChoiceIdAttachment() { comboBox.onChange = nullptr; }

    private:
        juce::ComboBox& comboBox;
        juce::ParameterAttachment attachment;
    };

    // Barra de quantidade bipolar: preenche do centro (0 %) até o valor, para a direita se
    // positivo e para a esquerda se negativo.
    class BipolarBarLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        BipolarBarLookAndFeel() : juce::LookAndFeel_V4 (juce::LookAndFeel_V4::getMidnightColourScheme()) {}

        void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                               float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle style,
                               juce::Slider& slider) override
        {
            if (style != juce::Slider::LinearBar)
            {
                juce::LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
                return;
            }

            const auto bounds = juce::Rectangle<float> (static_cast<float> (x), static_cast<float> (y),
                                                        static_cast<float> (width), static_cast<float> (height));
            const float centreX = bounds.getCentreX();

            g.setColour (slider.findColour (juce::Slider::backgroundColourId));
            g.fillRect (bounds);

            g.setColour (slider.findColour (juce::Slider::trackColourId));
            g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (centreX, sliderPos), bounds.getY() + 1.0f,
                                                                    juce::jmax (centreX, sliderPos), bounds.getBottom() - 1.0f));

            g.setColour (slider.findColour (juce::Slider::textBoxOutlineColourId));
            g.drawVerticalLine (juce::roundToInt (centreX), bounds.getY(), bounds.getBottom());
        }
    };
}

struct ModSlotList::Row final : public juce::Component
{
    Row (juce::AudioProcessorValueTreeState& apvts, ModulationModel& model, std::size_t slotIndex,
         juce::LookAndFeel& barLookAndFeel)
    {
        number.setText (juce::String (slotIndex + 1), juce::dontSendNotification);
        number.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (number);

        // Só as opções utilizáveis; as reservadas (no fim da lista) ficam escondidas.
        // O attachment usa a posição do item, que bate com o índice do parâmetro.
        for (std::size_t i = 0; i < ModMatrix::numUsableSources; ++i)
            source.addItem (ModMatrix::sourceNames[i], static_cast<int> (i) + 1);
        for (std::size_t i = 0; i < ModMatrix::numUsableDests; ++i)
            dest.addItem (ModMatrix::destNames[i], static_cast<int> (i) + 1);

        addAndMakeVisible (source);
        addAndMakeVisible (arrow);
        addAndMakeVisible (dest);

        arrow.setText (juce::String::fromUTF8 ("\xe2\x86\x92"), juce::dontSendNotification); // seta →
        arrow.setJustificationType (juce::Justification::centred);

        amount.setSliderStyle (juce::Slider::LinearBar);
        amount.setLookAndFeel (&barLookAndFeel);
        amount.setColour (juce::Slider::trackColourId, ModulationModel::accentColour().withAlpha (0.6f));
        amount.setTextValueSuffix (" %");
        addAndMakeVisible (amount);

        remove.setButtonText ("X");
        remove.setTooltip ("Clear this slot");
        remove.onClick = [&model, slotIndex] { model.clearSlot (slotIndex); };
        addAndMakeVisible (remove);

        sourceAttachment = std::make_unique<ChoiceIdAttachment> (*apvts.getParameter (ParamIDs::modSource[slotIndex]), source);
        destAttachment   = std::make_unique<ChoiceIdAttachment> (*apvts.getParameter (ParamIDs::modDest[slotIndex]), dest);
        amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, ParamIDs::modAmount[slotIndex], amount);
        amount.setDoubleClickReturnValue (true, 0.0);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (0, 1);
        number.setBounds (area.removeFromLeft (24));
        remove.setBounds (area.removeFromRight (area.getHeight() + 4).reduced (2, 0));

        const int columnWidth = (area.getWidth() - 24) / 3;
        source.setBounds (area.removeFromLeft (columnWidth).reduced (2, 0));
        arrow.setBounds (area.removeFromLeft (24));
        dest.setBounds (area.removeFromLeft (columnWidth).reduced (2, 0));
        amount.setBounds (area.reduced (2, 0));
    }

    juce::Label number, arrow;
    juce::ComboBox source, dest;
    juce::Slider amount;
    juce::TextButton remove;

    ~Row() override { amount.setLookAndFeel (nullptr); }

    std::unique_ptr<ChoiceIdAttachment> sourceAttachment, destAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;
};

ModSlotList::ModSlotList (juce::AudioProcessorValueTreeState& apvts, ModulationModel& model)
    : barLookAndFeel (std::make_unique<BipolarBarLookAndFeel>())
{
    for (auto* header : { &sourceHeader, &destHeader, &amountHeader })
    {
        header->setFont (juce::FontOptions (12.0f));
        header->setJustificationType (juce::Justification::centredLeft);
        header->setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.6f));
        addAndMakeVisible (*header);
    }

    sourceHeader.setText ("Source", juce::dontSendNotification);
    destHeader.setText ("Destination", juce::dontSendNotification);
    amountHeader.setText ("Amount", juce::dontSendNotification);

    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        rows[i] = std::make_unique<Row> (apvts, model, i, *barLookAndFeel);
        addAndMakeVisible (*rows[i]);
    }
}

ModSlotList::~ModSlotList() = default;

void ModSlotList::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (16);

    // Cabeçalho alinhado com as colunas das linhas.
    header.removeFromLeft (24);
    header.removeFromRight (header.getHeight() + 4);
    const int columnWidth = (header.getWidth() - 24) / 3;
    sourceHeader.setBounds (header.removeFromLeft (columnWidth));
    header.removeFromLeft (24);
    destHeader.setBounds (header.removeFromLeft (columnWidth));
    amountHeader.setBounds (header);

    const int rowHeight = area.getHeight() / static_cast<int> (rows.size());
    for (auto& row : rows)
        row->setBounds (area.removeFromTop (rowHeight));
}
