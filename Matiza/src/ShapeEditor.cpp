#include "ShapeEditor.h"
#include "Palette.h"

#include <cmath>

namespace
{
    constexpr int toolbarHeight = 22;
    constexpr float pointRadius = 4.0f;
    constexpr float hitRadius   = 7.0f;   // tolerância do clique em pontos e alças
    constexpr float pixelsPerFullCurve = 80.0f;

    // Opções da grade: ID do item no combo = divisões + 1 (0 não pode ser ID).
    constexpr int gridDivisions[] = { 0, 4, 8, 16 };
    const char* const gridNames[] = { "Grid off", "Grid 1/4", "Grid 1/8", "Grid 1/16" };
}

ShapeEditor::ShapeEditor()
{
    for (int i = 0; i < 4; ++i)
        gridBox.addItem (gridNames[i], i + 1);
    gridBox.setSelectedId (3, juce::dontSendNotification); // 1/8
    gridBox.onChange = [this] { repaint(); };
    addAndMakeVisible (gridBox);

    snapButton.setTooltip ("Snap points to the grid");
    addAndMakeVisible (snapButton);

    setTooltip ("Double-click: add or remove a point. Drag the middle handle of a segment to curve it.");
    startTimerHz (30);
}

void ShapeEditor::resized()
{
    auto area = getLocalBounds();
    auto toolbar = area.removeFromTop (toolbarHeight);
    gridBox.setBounds (toolbar.removeFromLeft (100).reduced (0, 1));
    toolbar.removeFromLeft (6);
    snapButton.setBounds (toolbar.removeFromLeft (70));

    canvas = area.toFloat().reduced (2.0f, 4.0f);
}

void ShapeEditor::timerCallback()
{
    // Só redesenha quando algo mudou: a forma (preset trocado, automação, outro editor), se é
    // editável, ou o ponto da fase.
    const bool nowEditable = editable();
    const float playhead = getPlayhead != nullptr ? getPlayhead() : -1.0f;

    if (draggingPoint < 0 && draggingCurve < 0 && getPoints != nullptr)
    {
        const auto current = getPoints();
        if (! (current == shape))
        {
            shape = current;
            repaint();
        }
    }

    if (nowEditable != drawnEditable || std::abs (playhead - drawnPlayhead) > 1.0e-4f)
        repaint();
}

juce::Point<float> ShapeEditor::toScreen (float x, float y) const
{
    return { canvas.getX() + x * canvas.getWidth(),
             canvas.getCentreY() - y * canvas.getHeight() / 2.0f };
}

juce::Point<float> ShapeEditor::toShape (juce::Point<float> screen) const
{
    const float x = (screen.x - canvas.getX()) / juce::jmax (1.0f, canvas.getWidth());
    const float y = (canvas.getCentreY() - screen.y) / juce::jmax (1.0f, canvas.getHeight() / 2.0f);
    return { juce::jlimit (0.0f, 1.0f, x), juce::jlimit (-1.0f, 1.0f, y) };
}

juce::Point<float> ShapeEditor::snapped (juce::Point<float> p) const
{
    const int divisions = gridDivisions[juce::jlimit (0, 3, gridBox.getSelectedId() - 1)];
    if (! snapButton.getToggleState() || divisions == 0)
        return p;

    // x na grade; altura em passos de 1/4 da faixa de cada lado.
    return { std::round (p.x * divisions) / divisions, std::round (p.y * 4.0f) / 4.0f };
}

juce::Point<float> ShapeEditor::handlePosition (int segment) const
{
    const float midX = (shape[segment].x + shape[segment + 1].x) / 2.0f;
    // evaluate num salto vertical devolve o valor depois do salto; a alça fica no meio da curva.
    const auto& a = shape[segment];
    const auto& b = shape[segment + 1];
    const float midY = a.y + (b.y - a.y) * static_cast<float> (ShapePoints::curveT (0.5, a.curve));
    return toScreen (midX, midY);
}

int ShapeEditor::pointAt (juce::Point<float> screen) const
{
    // Do último para o primeiro: em pontos sobrepostos (salto vertical), pega o de cima na lista.
    for (int i = shape.size() - 1; i >= 0; --i)
        if (toScreen (shape[i].x, shape[i].y).getDistanceFrom (screen) <= hitRadius)
            return i;
    return -1;
}

int ShapeEditor::handleAt (juce::Point<float> screen) const
{
    for (int i = 0; i < shape.size() - 1; ++i)
    {
        // Segmento vertical (salto) não tem curva para ajustar.
        if (shape[i + 1].x - shape[i].x <= 0.0f)
            continue;
        if (handlePosition (i).getDistanceFrom (screen) <= hitRadius)
            return i;
    }
    return -1;
}

void ShapeEditor::commit()
{
    if (onEdit != nullptr)
        onEdit (shape);
    repaint();
}

void ShapeEditor::mouseDown (const juce::MouseEvent& e)
{
    draggingPoint = draggingCurve = -1;
    if (! editable() || ! canvas.expanded (hitRadius).contains (e.position))
        return;

    if (getPoints != nullptr)
        shape = getPoints(); // parte da forma atual (a pronta, na primeira edição)

    const int point = pointAt (e.position);

    if (e.mods.isPopupMenu())
    {
        if (point >= 0 && shape.remove (point))
            commit();
        return;
    }

    if (point >= 0)
    {
        draggingPoint = point;
        return;
    }

    const int handle = handleAt (e.position);
    if (handle >= 0)
    {
        draggingCurve = handle;
        curveAtDragStart = shape[handle].curve;
    }
}

void ShapeEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingPoint >= 0)
    {
        const auto p = snapped (toShape (e.position));
        shape.move (draggingPoint, p.x, p.y);
        commit();
    }
    else if (draggingCurve >= 0)
    {
        // Arrastar para cima entorta a curva para cima, qualquer que seja o sentido do segmento.
        const auto& a = shape[draggingCurve];
        const auto& b = shape[draggingCurve + 1];
        const float direction = b.y >= a.y ? 1.0f : -1.0f;
        const float delta = static_cast<float> (e.getDistanceFromDragStartY()) / pixelsPerFullCurve;
        shape.setCurve (draggingCurve, curveAtDragStart + delta * direction);
        commit();
    }
}

void ShapeEditor::mouseUp (const juce::MouseEvent&)
{
    draggingPoint = draggingCurve = -1;
}

void ShapeEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    draggingPoint = draggingCurve = -1;
    if (! editable() || ! canvas.expanded (hitRadius).contains (e.position))
        return;

    if (getPoints != nullptr)
        shape = getPoints();

    if (const int point = pointAt (e.position); point >= 0)
    {
        if (shape.remove (point))
            commit();
        return;
    }

    if (const int handle = handleAt (e.position); handle >= 0)
    {
        shape.setCurve (handle, 0.0f);
        commit();
        return;
    }

    const auto p = snapped (toShape (e.position));
    if (shape.add (p.x, p.y) >= 0)
        commit();
}

void ShapeEditor::paint (juce::Graphics& g)
{
    drawnEditable = editable();
    drawnPlayhead = getPlayhead != nullptr ? getPlayhead() : -1.0f;

    // Fundo e linha do zero.
    g.setColour (Palette::background);
    g.fillRoundedRectangle (canvas.expanded (2.0f, 4.0f), 6.0f);

    g.setColour (Palette::text.withAlpha (0.15f));
    g.drawHorizontalLine (juce::roundToInt (canvas.getCentreY()), canvas.getX(), canvas.getRight());

    if (! drawnEditable)
    {
        g.setColour (Palette::textMuted);
        g.setFont (juce::FontOptions (13.0f));
        g.drawFittedText (notEditableMessage, canvas.toNearestInt(), juce::Justification::centred, 2);
        return;
    }

    // Grade vertical.
    const int divisions = gridDivisions[juce::jlimit (0, 3, gridBox.getSelectedId() - 1)];
    g.setColour (Palette::text.withAlpha (0.08f));
    for (int i = 1; i < divisions; ++i)
        g.drawVerticalLine (juce::roundToInt (canvas.getX() + canvas.getWidth() * i / divisions),
                            canvas.getY(), canvas.getBottom());

    // Curva: um ponto por pixel. Saltos verticais aparecem porque dois pontos têm o mesmo x.
    juce::Path path;
    const int numSteps = juce::jmax (2, juce::roundToInt (canvas.getWidth()));
    for (int i = 0; i <= numSteps; ++i)
    {
        const float x = static_cast<float> (i) / numSteps;
        const auto p = toScreen (x, shape.evaluate (x));
        if (i == 0)
            path.startNewSubPath (p);
        else
            path.lineTo (p);
    }
    g.setColour (Palette::primary);
    g.strokePath (path, juce::PathStrokeType (2.0f));

    // Alças de curva (anéis pequenos) e pontos (bolinhas).
    g.setColour (Palette::primary.withAlpha (0.6f));
    for (int i = 0; i < shape.size() - 1; ++i)
        if (shape[i + 1].x - shape[i].x > 0.0f)
            g.drawEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (handlePosition (i)), 1.2f);

    g.setColour (Palette::text);
    for (int i = 0; i < shape.size(); ++i)
        g.fillEllipse (juce::Rectangle<float> (pointRadius * 2.0f, pointRadius * 2.0f)
                           .withCentre (toScreen (shape[i].x, shape[i].y)));

    // Ponto da fase atual.
    if (drawnPlayhead >= 0.0f)
    {
        g.setColour (Palette::modulation);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f)
                           .withCentre (toScreen (drawnPlayhead, shape.evaluate (drawnPlayhead))));
    }
}
