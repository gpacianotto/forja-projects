#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "ModMatrix.h"

class ModulationModel;
class ModRingOverlay;

// Componentes da interface do MicroSynth. Cada controle se liga a um parâmetro do APVTS por um
// "Attachment", que sincroniza os dois lados (mexer no knob muda o parâmetro; automação no
// Ableton move o knob).
//
// Ordem dos membros importa: o Attachment é declarado DEPOIS do controle, então é construído
// depois e destruído antes dele (C++ destrói membros na ordem inversa da declaração).

// Knob giratório com nome em cima e valor embaixo.
//
// Um knob pode ser destino da matriz de modulação (RF10): aceita uma fonte arrastada
// (DragAndDropTarget ~ interface Java que o componente implementa) e mostra um anel com o
// alcance da modulação, que também pode ser arrastado para ajustar a quantidade.
class Knob final : public juce::Component, public juce::DragAndDropTarget
{
public:
    Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& labelText);
    ~Knob() override; // definido no .cpp, onde ModRingOverlay é um tipo completo

    // ringScale: quanto 100 % de modulação anda na faixa do knob (ex.: Pitch no knob de
    // Steps: 24 passos de uma faixa de 48 = 0,5).
    void enableModulation (ModulationModel& model, ModMatrix::Dest dest, float ringScale = 1.0f);

    void resized() override;
    void paintOverChildren (juce::Graphics&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;

private:
    juce::Label label;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;

    ModulationModel* modulationModel = nullptr;
    ModMatrix::Dest modDest = ModMatrix::Dest::none;
    std::unique_ptr<ModRingOverlay> ring;
    bool dropHighlight = false;
};

// Lista de opções (ex.: forma de onda) com nome em cima.
class Choice final : public juce::Component
{
public:
    Choice (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& labelText);
    void resized() override;

private:
    juce::Label label;
    juce::ComboBox comboBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

// Botão liga/desliga (parâmetro bool) com nome em cima.
class Toggle final : public juce::Component
{
public:
    Toggle (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& labelText);
    void resized() override;

    juce::ToggleButton& getButton() noexcept { return button; }

private:
    juce::Label label;
    juce::ToggleButton button;
    juce::AudioProcessorValueTreeState::ButtonAttachment attachment;
};

// Caixa com título que organiza seus controles lado a lado.
class ParameterGroup final : public juce::Component
{
public:
    explicit ParameterGroup (const juce::String& groupTitle);

    // O grupo vira dono do controle (unique_ptr ~ objeto com um único dono; apagado junto com o grupo).
    // Devolve uma referência ao controle, para quem precisar mexer nele depois (ex.: desabilitar).
    // "template" ~ método genérico do Java: T é o tipo concreto (Knob, Choice, Toggle...).
    template <typename ControlType>
    ControlType& add (std::unique_ptr<ControlType> control)
    {
        auto& ref = *control;
        addAndMakeVisible (ref);
        controls.push_back (std::move (control)); // std::move: transfere a posse, não copia
        return ref;
    }

    int getNumControls() const { return static_cast<int> (controls.size()); }

    // Componente opcional (ex.: um gráfico) desenhado acima da linha de controles.
    // O grupo vira dono dele, como dos controles.
    // heightFraction: parte da altura útil que o componente ocupa (o resto fica para os controles).
    template <typename DisplayType>
    DisplayType& setDisplay (std::unique_ptr<DisplayType> newDisplay, float heightFraction = 0.5f)
    {
        auto& ref = *newDisplay;
        addAndMakeVisible (ref);
        display = std::move (newDisplay);
        displayFraction = heightFraction;
        return ref;
    }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String title;
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::unique_ptr<juce::Component> display;
    float displayFraction = 0.5f;
};
