#pragma once

#include <array>
#include <functional>
#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "ModMatrix.h"
#include "Palette.h"
#include "Parameters.h"

// Interface da matriz de modulação (RF10).
//
// Tudo aqui roda na thread da interface. Os slots são parâmetros do APVTS, então mudar um slot
// pela interface é só mudar parâmetros: o áudio lê os mesmos valores pelos atômicos, e o estado
// é salvo com o set como qualquer outro parâmetro.

// Lê e altera os 8 slots. Usado pelos knobs (arrastar e soltar, anel) e pela lista de slots.
class ModulationModel
{
public:
    explicit ModulationModel (juce::AudioProcessorValueTreeState& apvts);

    ModMatrix::Slot getSlot (std::size_t slotIndex) const;

    // Liga a fonte ao destino. Se a ligação já existe, devolve o slot dela; senão ocupa o
    // primeiro slot livre com quantidade inicial de 50 %. Devolve -1 se os 8 estiverem ocupados.
    int addLink (ModMatrix::Source source, ModMatrix::Dest dest);

    void clearSlot (std::size_t slotIndex);

    // Slots que modulam o destino, em ordem de número (RF14: um anel para cada).
    juce::Array<int> slotsFor (ModMatrix::Dest dest) const;

    // Quantidade em % (-100 a +100). Gestos: begin/end avisam o host do início e do fim de um
    // arrasto, para a automação gravada e o desfazer do Ableton tratarem como um movimento só.
    float getAmountPercent (std::size_t slotIndex) const;
    void beginAmountGesture (std::size_t slotIndex);
    void setAmountPercent (std::size_t slotIndex, float percent);
    void endAmountGesture (std::size_t slotIndex);

    // Alcance de um slot em unidades normalizadas (-1 a +1): fonte unipolar empurra para um
    // lado; bipolar, para os dois.
    struct Range { float low = 0.0f; float high = 0.0f; };
    Range getSlotRange (std::size_t slotIndex) const;

    // Texto que viaja com o arrasto de uma fonte ("modSource:5") e o caminho de volta.
    static juce::var makeDragDescription (ModMatrix::Source source);
    static ModMatrix::Source parseDragDescription (const juce::var& description);

    // Cor de destaque da modulação (contorno de "solte aqui", barras da lista de slots).
    static juce::Colour accentColour() { return Palette::modulation; }

    // Cor de cada fonte (RF14): alças do cabeçalho e anéis dos knobs.
    static juce::Colour sourceColour (ModMatrix::Source source);

private:
    static void setParameter (juce::RangedAudioParameter& param, float value);

    std::array<juce::RangedAudioParameter*, ParamIDs::numModSlots> sourceParams {};
    std::array<juce::RangedAudioParameter*, ParamIDs::numModSlots> destParams {};
    std::array<juce::RangedAudioParameter*, ParamIDs::numModSlots> amountParams {};
};

// Anéis desenhados em volta de um knob modulado: um por slot (RF14), concêntricos, na cor da
// fonte. Fica por cima do slider, mas só captura o mouse sobre os anéis (hitTest); o resto do
// knob continua funcionando normalmente.
class ModRingOverlay final : public juce::Component, private juce::Timer
{
public:
    ModRingOverlay (ModulationModel& model, ModMatrix::Dest dest, juce::Slider& slider, float ringScale);
    ~ModRingOverlay() override { stopTimer(); }

    // Distância entre um anel e o próximo. O Knob encolhe o slider (número de anéis - 1) × isto.
    static constexpr float ringStep = 4.0f;

    int getNumRings() const noexcept { return drawnSlots.size(); }

    // Avisos para o Knob: o número de anéis mudou (refazer o layout) e clique direito num anel.
    std::function<void()> onRingCountChanged;
    std::function<void()> onPopupMenu;

    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    // radius = raio do anel de dentro; step = distância entre anéis.
    struct Geometry { juce::Point<float> centre; float radius = 0.0f; float step = ringStep; float startAngle = 0.0f; float endAngle = 0.0f; };
    Geometry getGeometry() const;

    // Anel (posição na lista drawnSlots) sob o ponto, ou -1.
    int ringAt (juce::Point<float> point) const;

    ModulationModel& model;
    ModMatrix::Dest dest;
    juce::Slider& slider;
    float scale;

    int draggingSlot = -1;
    float dragStartAmount = 0.0f;

    // Último estado desenhado, para só redesenhar quando algo mudar.
    double drawnProportion = -1.0;
    juce::Array<int> drawnSlots;
    std::array<float, ParamIDs::numModSlots> drawnAmounts {};
    std::array<int, ParamIDs::numModSlots> drawnSources {};
};

// Alça de uma fonte de modulação: arraste até um knob para criar a ligação.
class SourceChip final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SourceChip (ModMatrix::Source source);

    void paint (juce::Graphics&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    ModMatrix::Source source;
};

// Lista dos 8 slots: fonte, destino, quantidade e botão de apagar.
class ModSlotList final : public juce::Component
{
public:
    ModSlotList (juce::AudioProcessorValueTreeState& apvts, ModulationModel& model);
    ~ModSlotList() override;

    void resized() override;

private:
    // Visual da barra de quantidade (preenche a partir do centro). Declarado antes das
    // linhas: é destruído depois delas, que o usam.
    std::unique_ptr<juce::LookAndFeel> barLookAndFeel;

    struct Row; // definido no .cpp (pimpl: os detalhes ficam fora do .h)
    std::array<std::unique_ptr<Row>, ParamIDs::numModSlots> rows;
    juce::Label sourceHeader, destHeader, amountHeader;
};
