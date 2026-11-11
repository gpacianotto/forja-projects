#pragma once

#include <functional>

#include <juce_gui_basics/juce_gui_basics.h>

#include "ShapePoints.h"

// Editor de forma por pontos (RF15), usado pelos LFOs e pelos osciladores.
//
// O editor não sabe de onde vem a forma: o dono liga as funções abaixo (como callbacks em
// Java). Assim o mesmo componente serve para um LFO (ponto da fase atual, Sample & Hold não
// editável) e para um oscilador (aberto numa janela flutuante).
//
// Mouse:
//   - arrastar um ponto: move (o primeiro e o último só sobem e descem);
//   - duplo clique no vazio: cria um ponto; duplo clique ou clique direito num ponto: apaga;
//   - arrastar a alça no meio de um segmento: curva; duplo clique na alça: volta a reta.
class ShapeEditor final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    ShapeEditor();
    ~ShapeEditor() override { stopTimer(); }

    // Forma que o editor mostra agora (a desenhada ou os pontos da forma pronta escolhida).
    std::function<ShapePoints()> getPoints;
    // Chamado a cada mudança feita pelo usuário. O dono guarda os pontos e liga o Custom.
    std::function<void (const ShapePoints&)> onEdit;
    // false: mostra a mensagem em vez da forma e ignora o mouse (Sample & Hold).
    std::function<bool()> isEditable;
    juce::String notEditableMessage = "Not editable";
    // Fase (0..1) para o ponto que anda na forma, ou negativo para esconder. Opcional.
    std::function<float()> getPlayhead;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    // Conversão entre a forma (x 0..1, y -1..1) e os pixels da área de desenho.
    juce::Point<float> toScreen (float x, float y) const;
    juce::Point<float> toShape (juce::Point<float> screen) const; // x 0..1, y -1..1 (limitados)
    juce::Point<float> snapped (juce::Point<float> shapePos) const;

    int pointAt (juce::Point<float> screen) const;  // índice do ponto, ou -1
    int handleAt (juce::Point<float> screen) const; // índice do segmento (alça), ou -1
    juce::Point<float> handlePosition (int segment) const;

    bool editable() const { return isEditable == nullptr || isEditable(); }
    void commit(); // repassa a forma editada ao dono

    juce::ComboBox gridBox;
    juce::ToggleButton snapButton { "Snap" };
    juce::Rectangle<float> canvas;

    ShapePoints shape;          // o que está desenhado agora
    float drawnPlayhead = -1.0f;
    bool drawnEditable = true;

    int draggingPoint = -1;
    int draggingCurve = -1;
    float curveAtDragStart = 0.0f;
};
