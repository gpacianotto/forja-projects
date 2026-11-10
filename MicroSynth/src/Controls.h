#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Componentes da interface do MicroSynth. Cada controle se liga a um parâmetro do APVTS por um
// "Attachment", que sincroniza os dois lados (mexer no knob muda o parâmetro; automação no
// Ableton move o knob).
//
// Ordem dos membros importa: o Attachment é declarado DEPOIS do controle, então é construído
// depois e destruído antes dele (C++ destrói membros na ordem inversa da declaração).

// Knob giratório com nome em cima e valor embaixo.
class Knob final : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& labelText);
    void resized() override;

private:
    juce::Label label;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
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
    template <typename DisplayType>
    DisplayType& setDisplay (std::unique_ptr<DisplayType> newDisplay)
    {
        auto& ref = *newDisplay;
        addAndMakeVisible (ref);
        display = std::move (newDisplay);
        return ref;
    }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::String title;
    std::vector<std::unique_ptr<juce::Component>> controls;
    std::unique_ptr<juce::Component> display;
};
