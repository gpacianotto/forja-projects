#include "TuningStepsView.h"

#include <algorithm>
#include <cmath>

#include "Palette.h"
#include "Parameters.h"
#include "StepMath.h"

namespace
{
    constexpr int    refreshHz      = 30;
    constexpr double glowFadeSeconds = 0.3; // RF16: o brilho some ~300 ms depois de soltar

    // Propriedades do estado do plugin com as escolhas da interface (não mudam o som).
    const juce::Identifier twelveTetId { "stepsTwelveTet" };
    const juce::Identifier viewId      { "stepsView" };

    // Caracteres fora do ASCII escritos em UTF-8 explícito: assim o MSVC não depende da
    // codificação do arquivo-fonte.
    const juce::String centSign   = juce::String (juce::CharPointer_UTF8 ("\xc2\xa2"));     // ¢
    const juce::String arrow      = juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92")); // →
    const juce::String middleDot  = juce::String (juce::CharPointer_UTF8 ("\xc2\xb7"));     // ·
    const juce::String primeSign  = juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xb2")); // ′

    // Ponto a `radius` do centro, no ângulo dado (0 no topo, sentido horário).
    juce::Point<float> polar (juce::Point<float> centre, double angle, float radius)
    {
        return { centre.x + radius * static_cast<float> (std::sin (angle)),
                 centre.y - radius * static_cast<float> (std::cos (angle)) };
    }

    juce::String formatHz (double hz)
    {
        return hz < 1000.0 ? juce::String (juce::roundToInt (hz)) + " Hz"
                           : juce::String (hz / 1000.0, 2) + " kHz";
    }

    juce::String signedInt (int value)
    {
        return (value >= 0 ? "+" : "") + juce::String (value);
    }

    // Fundo dos gráficos, igual ao de Graphs.cpp: um pouco mais escuro que o grupo.
    juce::Colour graphBackground() { return Palette::background.darker (0.3f); }
}

TuningStepsView::TuningStepsView (PluginProcessor& processor)
    : pluginProcessor (processor), apvts (processor.apvts)
{
    edoParam     = apvts.getRawParameterValue (ParamIDs::edo);
    baseHzParam  = apvts.getRawParameterValue (ParamIDs::baseHz);
    refNoteParam = apvts.getRawParameterValue (ParamIDs::refNote);

    dotCents.fill (-1.0f);
    heldNotes.reserve (PluginProcessor::numVoices);

    // Sub-abas Circle | Line: botões de rádio (só um ligado por vez, como um RadioButton do Swing).
    for (auto* button : { &circleButton, &lineButton })
    {
        button->setClickingTogglesState (true);
        button->setRadioGroupId (1);
        button->setColour (juce::TextButton::buttonColourId, Palette::widget);
        button->setColour (juce::TextButton::buttonOnColourId, Palette::primary);
        button->setColour (juce::TextButton::textColourOffId, Palette::text);
        button->setColour (juce::TextButton::textColourOnId, Palette::background);
        addAndMakeVisible (*button);
    }
    circleButton.onClick = [this] { if (circleButton.getToggleState()) setView (View::circle, true); };
    lineButton.onClick   = [this] { if (lineButton.getToggleState())   setView (View::line, true); };

    twelveTetButton.setTooltip ("Show the 12 notes of standard tuning (12-TET) for comparison.");
    twelveTetButton.setColour (juce::ToggleButton::tickColourId, Palette::primary);
    twelveTetButton.onClick = [this] { setTwelveTet (twelveTetButton.getToggleState(), true); };
    addAndMakeVisible (twelveTetButton);

    syncUiStateFromTree();
    circleButton.setToggleState (view == View::circle, juce::dontSendNotification);
    lineButton.setToggleState (view == View::line, juce::dontSendNotification);

    timerCallback(); // primeiro estado já na abertura
    startTimerHz (refreshHz);
}

// ----------------------------------------------------------------------------- Estado da interface

void TuningStepsView::setView (View newView, bool save)
{
    if (save)
        apvts.state.setProperty (viewId, newView == View::line ? "line" : "circle", nullptr);

    if (newView == view)
        return;

    view = newView;
    circleButton.setToggleState (view == View::circle, juce::dontSendNotification);
    lineButton.setToggleState (view == View::line, juce::dontSendNotification);
    repaint();
}

void TuningStepsView::setTwelveTet (bool on, bool save)
{
    if (save)
        apvts.state.setProperty (twelveTetId, on, nullptr);

    if (on == showTwelveTet)
        return;

    showTwelveTet = on;
    twelveTetButton.setToggleState (on, juce::dontSendNotification);
    repaint();
}

void TuningStepsView::syncUiStateFromTree()
{
    // Ao carregar um set, o estado inteiro é trocado: confere a cada quadro (é barato).
    const bool savedTwelveTet = static_cast<bool> (apvts.state.getProperty (twelveTetId, false));
    const View savedView = apvts.state.getProperty (viewId, "circle").toString() == "line" ? View::line : View::circle;

    setTwelveTet (savedTwelveTet, false);
    setView (savedView, false);
}

// ----------------------------------------------------------------------------- Timer

void TuningStepsView::timerCallback()
{
    syncUiStateFromTree();

    bool changed = false;

    const int    newEdo     = juce::jlimit (1, maxEdo, juce::roundToInt (edoParam->load()));
    const double newBaseHz  = static_cast<double> (baseHzParam->load());
    const int    newRefNote = juce::roundToInt (refNoteParam->load());

    if (newEdo != edo || newBaseHz != baseHz || newRefNote != refNote)
    {
        edo = newEdo;
        baseHz = newBaseHz;
        refNote = newRefNote;
        changed = true;
    }

    // Notas seguradas agora: quais steps acendem, onde ficam os pontos e a lista.
    std::array<bool, maxEdo> held {};
    std::array<float, PluginProcessor::numVoices> newDots {};
    newDots.fill (-1.0f);

    std::vector<HeldNote> newHeld;
    newHeld.reserve (PluginProcessor::numVoices);

    for (size_t i = 0; i < pluginProcessor.voicePitchDisplay.size(); ++i)
    {
        const auto& voice = pluginProcessor.voicePitchDisplay[i];
        if (! voice.held.load())
            continue;

        const int note = voice.midiNote.load();
        const auto pos = StepMath::locate (note, refNote, edo);
        held[static_cast<size_t> (pos.step)] = true;
        newDots[i] = static_cast<float> (StepMath::wrapCents (voice.centsFromBase.load()));

        const bool alreadyListed = std::any_of (newHeld.begin(), newHeld.end(),
                                                [note] (const HeldNote& h) { return h.midiNote == note; });
        if (! alreadyListed)
            newHeld.push_back ({ note, pos.step, pos.octave });
    }

    std::sort (newHeld.begin(), newHeld.end(),
               [] (const HeldNote& a, const HeldNote& b) { return a.midiNote < b.midiNote; });

    for (int k = 0; k < maxEdo; ++k)
    {
        const auto index = static_cast<size_t> (k);
        // Steps que não existem nesta EDO apagam na hora.
        const float next = k < edo ? StepMath::nextGlow (glow[index], held[index], 1.0 / refreshHz, glowFadeSeconds)
                                   : 0.0f;
        if (next != glow[index])
        {
            glow[index] = next;
            changed = true;
        }
    }

    if (newDots != dotCents)
    {
        dotCents = newDots;
        changed = true;
    }

    if (newHeld != heldNotes)
    {
        heldNotes = std::move (newHeld);
        changed = true;
    }

    if (changed)
        repaint();
}

// ----------------------------------------------------------------------------- Layout

void TuningStepsView::resized()
{
    auto area = getLocalBounds();
    headerArea = area.removeFromTop (24);

    auto buttons = headerArea;
    lineButton.setBounds (buttons.removeFromRight (56).reduced (0, 1));
    buttons.removeFromRight (4);
    circleButton.setBounds (buttons.removeFromRight (64).reduced (0, 1));
    buttons.removeFromRight (12);
    twelveTetButton.setBounds (buttons.removeFromRight (78));

    // O botão Circle/Line ocupa só os cantos; o resto do cabeçalho é desenhado em paint().
    headerArea = buttons;
}

void TuningStepsView::paint (juce::Graphics& g)
{
    paintHeader (g, headerArea.toFloat());

    auto area = getLocalBounds().toFloat();
    area.removeFromTop (30.0f);

    if (view == View::circle)
        paintCircle (g, area);
    else
        paintLine (g, area);
}

void TuningStepsView::paintHeader (juce::Graphics& g, juce::Rectangle<float> area)
{
    // Ex.: "19 EDO · 63.2 ¢/step"
    g.setColour (Palette::text);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (juce::String (edo) + " EDO " + middleDot + " "
                    + juce::String (StepMath::stepCents (1, edo), 1) + " " + centSign + "/step",
                area, juce::Justification::centredLeft);
}

// ----------------------------------------------------------------------------- Linhas

void TuningStepsView::drawStepLine (juce::Graphics& g, juce::Line<float> line, int step) const
{
    const float b = glow[static_cast<size_t> (step)];

    if (b > 0.0f)
    {
        // "Brilho" sem desfoque: duas linhas largas e translúcidas por baixo da linha principal.
        g.setColour (Palette::primary.withAlpha (0.15f * b));
        g.drawLine (line, 10.0f * b);
        g.setColour (Palette::primary.withAlpha (0.30f * b));
        g.drawLine (line, 5.5f * b);
    }

    const auto base = step == 0 ? Palette::secondary : Palette::textMuted.withAlpha (0.8f);
    const float baseThickness = step == 0 ? 2.0f : 1.2f;

    g.setColour (base.interpolatedWith (Palette::primary, b));
    g.drawLine (line, baseThickness + (3.5f - baseThickness) * b);
}

juce::Colour TuningStepsView::labelColour (int step) const
{
    if (glow[static_cast<size_t> (step)] > 0.0f)
        return Palette::primary;

    return step == 0 ? Palette::secondary : Palette::textMuted;
}

// ----------------------------------------------------------------------------- Circle

void TuningStepsView::paintCircle (juce::Graphics& g, juce::Rectangle<float> area)
{
    // Lista das notas à direita; o círculo fica com o resto.
    const float listWidth = juce::jmin (250.0f, area.getWidth() * 0.4f);
    auto listArea = area.removeFromRight (listWidth);
    paintHeldList (g, listArea.reduced (8.0f, 4.0f), false);

    constexpr float labelMargin = 22.0f; // espaço para os números fora da borda
    const float radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f - labelMargin;
    if (radius < 20.0f)
        return;

    const auto centre = area.getCentre();

    // Disco
    g.setColour (graphBackground());
    g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));
    g.setColour (Palette::outline);
    g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), 1.0f);

    // Referência 12-TET: tracinhos fora da borda, a cada 100 ¢.
    if (showTwelveTet)
    {
        g.setColour (Palette::textMuted.withAlpha (0.7f));
        for (int i = 0; i < 12; ++i)
        {
            const double angle = StepMath::centsToAngle (100.0 * i);
            g.drawLine ({ polar (centre, angle, radius + 3.0f), polar (centre, angle, radius + 9.0f) }, 1.5f);
        }
    }

    // Linhas: as apagadas primeiro, as acesas por cima.
    for (const bool litPass : { false, true })
        for (int k = 0; k < edo; ++k)
            if ((glow[static_cast<size_t> (k)] > 0.0f) == litPass)
            {
                const double angle = StepMath::centsToAngle (StepMath::stepCents (k, edo));
                drawStepLine (g, { centre, polar (centre, angle, radius) }, k);
            }

    // Números dos steps, fora da borda.
    g.setFont (juce::FontOptions (11.0f));
    for (int k = 0; k < edo; ++k)
    {
        if (! StepMath::shouldLabel (k, edo) && glow[static_cast<size_t> (k)] <= 0.0f)
            continue;

        const double angle = StepMath::centsToAngle (StepMath::stepCents (k, edo));
        const auto at = polar (centre, angle, radius + 15.0f);
        g.setColour (labelColour (k));
        g.drawText (juce::String (k), juce::Rectangle<float> (26.0f, 14.0f).withCentre (at),
                    juce::Justification::centred);
    }

    // Miolo com o nome da EDO.
    const float hubRadius = juce::jmin (34.0f, radius * 0.32f);
    const auto hub = juce::Rectangle<float> (hubRadius * 2.0f, hubRadius * 2.0f).withCentre (centre);
    g.setColour (Palette::panel);
    g.fillEllipse (hub);
    g.setColour (Palette::outline);
    g.drawEllipse (hub, 1.0f);

    if (hubRadius >= 26.0f)
    {
        g.setColour (Palette::text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (juce::String (edo) + " EDO", hub.withTrimmedBottom (hubRadius * 0.7f).withTrimmedTop (hubRadius * 0.25f),
                    juce::Justification::centredBottom);
        g.setColour (Palette::textMuted);
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (juce::String (StepMath::stepCents (1, edo), 1) + " " + centSign + "/step",
                    hub.withTrimmedTop (hubRadius * 1.05f), juce::Justification::centredTop);
    }

    // Pontos: a altura que cada voz segurada está tocando (anda com pitch bend e glide).
    for (const float cents : dotCents)
    {
        if (cents < 0.0f)
            continue;

        const auto at = polar (centre, StepMath::centsToAngle (cents), radius);
        g.setColour (Palette::primary.withAlpha (0.3f));
        g.fillEllipse (juce::Rectangle<float> (18.0f, 18.0f).withCentre (at));
        g.setColour (Palette::primary);
        g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (at));
    }
}

// ----------------------------------------------------------------------------- Line

void TuningStepsView::paintLine (juce::Graphics& g, juce::Rectangle<float> area)
{
    auto listArea = area.removeFromBottom (22.0f);
    area = area.reduced (20.0f, 0.0f); // folga para os textos das pontas
    paintHeldList (g, listArea.withX (area.getX()).withRight (area.getRight()), true);

    auto hzRow    = area.removeFromTop (16.0f);
    auto ruler    = area.removeFromBottom (32.0f);
    auto bar      = area.reduced (0.0f, 2.0f);
    if (bar.getHeight() < 30.0f || bar.getWidth() < 100.0f)
        return;

    auto xOf = [&bar] (double cents)
    {
        return bar.getX() + static_cast<float> (StepMath::centsToFraction (cents)) * bar.getWidth();
    };

    g.setColour (graphBackground());
    g.fillRect (bar);
    g.setColour (Palette::outline);
    g.drawRect (bar, 1.0f);

    auto lineArea = bar.reduced (0.0f, 4.0f);
    auto numbersRow = lineArea.removeFromBottom (16.0f); // números dos steps, dentro da barra
    lineArea.removeFromBottom (2.0f);

    // Referência 12-TET: linhas tracejadas a cada 100 ¢.
    if (showTwelveTet)
    {
        g.setColour (Palette::textMuted.withAlpha (0.35f));
        const float dashes[] = { 3.0f, 3.0f };
        for (int c = 100; c < 1200; c += 100)
            g.drawDashedLine ({ xOf (c), lineArea.getY(), xOf (c), lineArea.getBottom() }, dashes, 2, 1.0f);
    }

    // Linhas: k = edo é o step 0 da oitava seguinte (0′), na borda direita.
    for (const bool litPass : { false, true })
        for (int k = 0; k <= edo; ++k)
        {
            const int step = k % edo;
            if ((glow[static_cast<size_t> (step)] > 0.0f) != litPass)
                continue;

            const float x = xOf (k == edo ? StepMath::centsPerOctave : StepMath::stepCents (k, edo));
            drawStepLine (g, { x, lineArea.getY(), x, lineArea.getBottom() }, step);
        }

    // Números dos steps
    g.setFont (juce::FontOptions (11.0f));
    for (int k = 0; k <= edo; ++k)
    {
        const int step = k % edo;
        if (! StepMath::shouldLabel (step, edo) && glow[static_cast<size_t> (step)] <= 0.0f)
            continue;

        const float x = xOf (k == edo ? StepMath::centsPerOctave : StepMath::stepCents (k, edo));
        g.setColour (labelColour (step));
        g.drawText (k == edo ? "0" + primeSign : juce::String (k),
                    juce::Rectangle<float> (x - 13.0f, numbersRow.getY(), 26.0f, numbersRow.getHeight()),
                    juce::Justification::centred);
    }

    // Régua em cents embaixo: marca a cada 100 ¢; texto a cada 100 ¢ se couber, senão a cada 300 ¢.
    const int textEvery = bar.getWidth() / 12.0f >= 40.0f ? 100 : 300;
    g.setFont (juce::FontOptions (10.5f));
    for (int c = 0; c <= 1200; c += 100)
    {
        const float x = xOf (c);
        g.setColour (Palette::textMuted);
        g.drawLine (x, ruler.getY(), x, ruler.getY() + 5.0f, 1.0f);

        if (c % textEvery == 0)
            g.drawText (juce::String (c) + centSign, juce::Rectangle<float> (x - 24.0f, ruler.getY() + 6.0f, 48.0f, 14.0f),
                        juce::Justification::centred);
    }

    // Régua em Hz em cima: a oitava de referência, de Base Freq até 2 × Base Freq.
    g.setColour (Palette::textMuted);
    for (int c = 0; c <= 1200; c += 300)
        g.drawText (formatHz (StepMath::toHz (baseHz, 0, c)),
                    juce::Rectangle<float> (xOf (c) - 32.0f, hzRow.getY(), 64.0f, hzRow.getHeight()),
                    juce::Justification::centred);

    // Pontos das vozes seguradas, no topo da barra.
    for (const float cents : dotCents)
    {
        if (cents < 0.0f)
            continue;

        const juce::Point<float> at { xOf (cents), lineArea.getY() };
        g.setColour (Palette::primary.withAlpha (0.3f));
        g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (at));
        g.setColour (Palette::primary);
        g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (at));
    }
}

// ----------------------------------------------------------------------------- Lista de notas

void TuningStepsView::paintHeldList (juce::Graphics& g, juce::Rectangle<float> area, bool singleRow)
{
    // Ex.: "MIDI 75 → step 6, oct +0 · 379 ¢ · 547.7 Hz"
    auto describe = [this] (const HeldNote& note)
    {
        const double cents = StepMath::stepCents (note.step, edo);
        return "MIDI " + juce::String (note.midiNote) + " " + arrow + " step " + juce::String (note.step)
             + ", oct " + signedInt (note.octave) + " " + middleDot + " " + juce::String (juce::roundToInt (cents))
             + " " + centSign + " " + middleDot + " " + juce::String (StepMath::toHz (baseHz, note.octave, cents), 1) + " Hz";
    };

    auto drawItem = [&g] (juce::Rectangle<float> row, const juce::String& text)
    {
        g.setColour (Palette::primary);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ row.getX() + 4.0f, row.getCentreY() }));
        g.setColour (Palette::text);
        g.drawText (text, row.withTrimmedLeft (14.0f), juce::Justification::centredLeft);
    };

    g.setFont (juce::FontOptions (12.0f));

    if (singleRow)
    {
        if (heldNotes.empty())
        {
            g.setColour (Palette::textMuted);
            g.drawText ("Play a note to light its step.", area, juce::Justification::centredLeft);
            return;
        }

        // Lado a lado; o que não couber vira "+N".
        constexpr float itemWidth = 270.0f;
        const int fits = juce::jmax (1, static_cast<int> ((area.getWidth() - 40.0f) / itemWidth));
        const int shown = juce::jmin (fits, static_cast<int> (heldNotes.size()));

        for (int i = 0; i < shown; ++i)
            drawItem (area.removeFromLeft (itemWidth), describe (heldNotes[static_cast<size_t> (i)]));

        if (shown < static_cast<int> (heldNotes.size()))
        {
            g.setColour (Palette::textMuted);
            g.drawText ("+" + juce::String (static_cast<int> (heldNotes.size()) - shown), area,
                        juce::Justification::centredLeft);
        }
        return;
    }

    g.setColour (Palette::textMuted);
    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    g.drawText ("Notes held", area.removeFromTop (20.0f), juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (12.0f));

    if (heldNotes.empty())
    {
        g.drawText ("Play a note to light its step.", area.removeFromTop (20.0f), juce::Justification::centredLeft);
        return;
    }

    for (const auto& note : heldNotes)
    {
        if (area.getHeight() < 20.0f)
            break;
        drawItem (area.removeFromTop (20.0f), describe (note));
    }
}
