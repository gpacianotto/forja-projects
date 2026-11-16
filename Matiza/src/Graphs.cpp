#include "Graphs.h"
#include "GraphMath.h"
#include "Oscillator.h"
#include "Palette.h"
#include "Parameters.h"
#include "Unison.h"

namespace
{
    constexpr int refreshHz = 30;

    // Fundo comum dos gráficos: retângulo arredondado um pouco mais escuro que o grupo.
    // Devolve a área útil para desenhar a curva.
    juce::Rectangle<float> drawGraphBackground (juce::Graphics& g, const juce::Component& c)
    {
        const auto bounds = c.getLocalBounds().toFloat().reduced (2.0f);
        const auto& lf = c.getLookAndFeel();

        g.setColour (lf.findColour (juce::ResizableWindow::backgroundColourId).darker (0.3f));
        g.fillRoundedRectangle (bounds, 6.0f);

        return bounds.reduced (6.0f, 6.0f);
    }

    // Cor de destaque das curvas: a cor principal da paleta (palha escura da Têmpera).
    juce::Colour curveColour (const juce::Component&)
    {
        return Palette::primary;
    }

    juce::Colour gridColour (const juce::Component& c)
    {
        return c.getLookAndFeel().findColour (juce::Label::textColourId).withAlpha (0.15f);
    }

    juce::Colour gridTextColour (const juce::Component& c)
    {
        return c.getLookAndFeel().findColour (juce::Label::textColourId).withAlpha (0.45f);
    }
}

// ----------------------------------------------------------------------------- FilterGraph

namespace
{
    constexpr double minHz = 20.0, maxHz = 20000.0;
    constexpr double topDb = 24.0, bottomDb = -36.0;

    // Frequência -> x (escala log): cada década ocupa a mesma largura.
    float hzToX (double hz, juce::Rectangle<float> area)
    {
        const double t = std::log (hz / minHz) / std::log (maxHz / minHz);
        return area.getX() + static_cast<float> (t) * area.getWidth();
    }

    double xToHz (float x, juce::Rectangle<float> area)
    {
        const double t = (x - area.getX()) / area.getWidth();
        return minHz * std::pow (maxHz / minHz, t);
    }

    float dbToY (double db, juce::Rectangle<float> area)
    {
        const double t = (topDb - juce::jlimit (bottomDb, topDb, db)) / (topDb - bottomDb);
        return area.getY() + static_cast<float> (t) * area.getHeight();
    }
}

FilterGraph::FilterGraph (juce::AudioProcessorValueTreeState& apvts, const ModulationDisplay& modulationDisplay)
    : modulation (modulationDisplay)
{
    cutoffParam = apvts.getRawParameterValue (ParamIDs::cutoff);
    resoParam   = apvts.getRawParameterValue (ParamIDs::reso);
    cutoffRange = apvts.getParameterRange (ParamIDs::cutoff);
    resoRange   = apvts.getParameterRange (ParamIDs::reso);
    startTimerHz (refreshHz);
}

FilterGraph::Values FilterGraph::modulatedValues() const
{
    // Mesma conta da voz (MatizaVoice::updateModulation): soma o deslocamento na posição
    // normalizada do knob e converte de volta para Hz / Q.
    auto modulate = [] (const juce::NormalisableRange<float>& range, float base, std::optional<float> offset)
    {
        if (! offset.has_value() || *offset == 0.0f)
            return base;
        return range.convertFrom0to1 (ModMatrix::applyNormalized (range.convertTo0to1 (base), *offset));
    };

    return { modulate (cutoffRange, cutoffParam->load(), modulation.read (ModMatrix::Dest::cutoff)),
             modulate (resoRange,   resoParam->load(),   modulation.read (ModMatrix::Dest::resonance)) };
}

void FilterGraph::timerCallback()
{
    if (cutoffParam->load() != drawnCutoff || resoParam->load() != drawnReso || ! (modulatedValues() == drawnModulated))
        repaint();
}

void FilterGraph::paint (juce::Graphics& g)
{
    drawnCutoff    = cutoffParam->load();
    drawnReso      = resoParam->load();
    drawnModulated = modulatedValues();
    const bool modulated = drawnModulated.cutoff != drawnCutoff || drawnModulated.reso != drawnReso;

    const auto area = drawGraphBackground (g, *this);

    // Grade: 100 Hz, 1 kHz e 10 kHz na vertical; 0 dB na horizontal.
    g.setFont (juce::FontOptions (11.0f));
    for (const auto& [hz, text] : { std::pair { 100.0, "100" }, std::pair { 1000.0, "1k" }, std::pair { 10000.0, "10k" } })
    {
        const float x = hzToX (hz, area);
        g.setColour (gridColour (*this));
        g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());
        g.setColour (gridTextColour (*this));
        g.drawText (text, juce::Rectangle<float> (x + 3.0f, area.getBottom() - 14.0f, 30.0f, 14.0f),
                    juce::Justification::bottomLeft);
    }

    const float zeroDbY = dbToY (0.0, area);
    g.setColour (gridColour (*this));
    g.drawHorizontalLine (juce::roundToInt (zeroDbY), area.getX(), area.getRight());
    g.setColour (gridTextColour (*this));
    g.drawText ("0 dB", juce::Rectangle<float> (area.getX() + 3.0f, zeroDbY - 14.0f, 40.0f, 14.0f),
                juce::Justification::bottomLeft);

    // Curva: um ponto a cada 2 pixels.
    // (Não dá para usar curve.isEmpty() para achar o primeiro ponto: um Path só com o ponto
    // inicial ainda conta como vazio, e cada ponto abriria um caminho novo, sem linhas.)
    auto makeCurve = [&area] (float cutoff, float reso)
    {
        juce::Path path;
        bool firstPoint = true;
        for (float x = area.getX(); x <= area.getRight(); x += 2.0f)
        {
            const double db = GraphMath::lowpassMagnitudeDb (xToHz (x, area), cutoff, reso);
            const float y = dbToY (db, area);

            if (firstPoint)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);

            firstPoint = false;
        }
        return path;
    };

    // RF20: com modulação, a curva dos knobs fica apagada por trás da curva modulada.
    if (modulated)
    {
        g.setColour (curveColour (*this).withAlpha (0.3f));
        g.strokePath (makeCurve (drawnCutoff, drawnReso), juce::PathStrokeType (1.5f));
    }

    const juce::Path curve = makeCurve (drawnModulated.cutoff, drawnModulated.reso);

    // Preenchimento translúcido embaixo da curva.
    juce::Path fill (curve);
    fill.lineTo (area.getRight(), area.getBottom());
    fill.lineTo (area.getX(), area.getBottom());
    fill.closeSubPath();
    g.setColour (curveColour (*this).withAlpha (0.2f));
    g.fillPath (fill);

    g.setColour (curveColour (*this));
    g.strokePath (curve, juce::PathStrokeType (2.0f));
}

// ----------------------------------------------------------------------------- OscillatorGraph

OscillatorGraph::OscillatorGraph (juce::AudioProcessorValueTreeState& apvts, std::size_t oscIndex,
                                  std::function<ShapePoints()> custom,
                                  std::function<void (juce::Component&)> open)
    : customPoints (std::move (custom)), openEditor (std::move (open))
{
    onParam    = apvts.getRawParameterValue (ParamIDs::oscOn[oscIndex]);
    waveParam  = apvts.getRawParameterValue (ParamIDs::oscWave[oscIndex]);
    levelParam = apvts.getRawParameterValue (ParamIDs::oscLevel[oscIndex]);
    setTooltip ("Click to draw the waveform");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (refreshHz);
}

void OscillatorGraph::timerCallback()
{
    const bool isCustom = juce::roundToInt (waveParam->load()) == static_cast<int> (Waveform::custom);
    const bool pointsChanged = isCustom && customPoints != nullptr && ! (customPoints() == drawnPoints);

    if (pointsChanged || onParam->load() != drawnOn || waveParam->load() != drawnWave || levelParam->load() != drawnLevel)
        repaint();
}

void OscillatorGraph::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && openEditor != nullptr)
        openEditor (*this);
}

void OscillatorGraph::paint (juce::Graphics& g)
{
    drawnOn    = onParam->load();
    drawnWave  = waveParam->load();
    drawnLevel = levelParam->load();

    // Deixa livre a faixa do nome (18 px), para o gráfico alinhar com os combos e knobs vizinhos.
    auto bounds = getLocalBounds().toFloat().withTrimmedTop (18.0f).reduced (2.0f, 4.0f);
    g.setColour (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId).darker (0.3f));
    g.fillRoundedRectangle (bounds, 6.0f);
    const auto area = bounds.reduced (6.0f, 6.0f);

    // Linha do zero.
    g.setColour (gridColour (*this));
    g.drawHorizontalLine (juce::roundToInt (area.getCentreY()), area.getX(), area.getRight());

    // Um ciclo gerado pelo próprio Oscillator do synth (a mesma onda que soa), com uma
    // amostra por pixel: frequência 1 Hz numa "taxa de amostragem" igual à largura.
    // Com Custom, desenha os pontos (a forma como foi desenhada).
    const bool isCustom = juce::roundToInt (drawnWave) == static_cast<int> (Waveform::custom);
    if (isCustom && customPoints != nullptr)
        drawnPoints = customPoints();

    Oscillator oscillator;
    oscillator.setWaveform (static_cast<Waveform> (juce::roundToInt (drawnWave)));
    const int numPoints = juce::jmax (2, juce::roundToInt (area.getWidth()));
    oscillator.setFrequency (1.0, numPoints);

    auto nextValue = [&] (int i)
    {
        return isCustom ? drawnPoints.evaluate (static_cast<double> (i) / numPoints) : oscillator.nextSample();
    };

    const float amplitude = juce::jlimit (0.0f, 1.0f, drawnLevel / 100.0f) * area.getHeight() / 2.0f;

    juce::Path wave;
    for (int i = 0; i <= numPoints; ++i)
    {
        const float x = area.getX() + area.getWidth() * static_cast<float> (i) / static_cast<float> (numPoints);
        const float y = area.getCentreY() - nextValue (i) * amplitude;

        if (i == 0)
            wave.startNewSubPath (x, y);
        else
            wave.lineTo (x, y);
    }

    const bool enabled = drawnOn > 0.5f;
    g.setColour (curveColour (*this).withAlpha (enabled ? 1.0f : 0.3f));
    g.strokePath (wave, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
}

// ----------------------------------------------------------------------------- UnisonGraph

UnisonGraph::UnisonGraph (juce::AudioProcessorValueTreeState& apvts, std::size_t oscIndex,
                          std::function<ScaleLines::Lines()> lines)
    : scaleLines (std::move (lines))
{
    onParam     = apvts.getRawParameterValue (ParamIDs::oscOn[oscIndex]);
    unisonParam = apvts.getRawParameterValue (ParamIDs::oscUnison[oscIndex]);
    detuneParam = apvts.getRawParameterValue (ParamIDs::oscDetune[oscIndex]);
    blendParam  = apvts.getRawParameterValue (ParamIDs::oscBlend[oscIndex]);
    widthParam  = apvts.getRawParameterValue (ParamIDs::oscWidth[oscIndex]);
    startTimerHz (refreshHz);
}

std::array<float, 5> UnisonGraph::readValues() const
{
    return { onParam->load(), unisonParam->load(), detuneParam->load(), blendParam->load(), widthParam->load() };
}

void UnisonGraph::timerCallback()
{
    // Knobs, automação e a escala (EDO, afinação desenhada, master do MTS-ESP) mudam o desenho.
    const auto lines = scaleLines != nullptr ? scaleLines() : ScaleLines::Lines {};
    if (readValues() != drawnValues || ! (lines == drawnLines))
        repaint();
}

void UnisonGraph::paint (juce::Graphics& g)
{
    drawnValues = readValues();
    drawnLines  = scaleLines != nullptr ? scaleLines() : ScaleLines::Lines {};

    const bool  enabled = drawnValues[0] > 0.5f;
    const int   count   = juce::jlimit (1, Unison::maxVoices, juce::roundToInt (drawnValues[1]));
    const float detune  = drawnValues[2];
    const float blend   = drawnValues[3] / 100.0f;
    const float width   = drawnValues[4] / 100.0f;
    const auto  layout  = Unison::computeLayout (count, detune, blend, width); // mesma conta do áudio
    const float alpha   = enabled ? 1.0f : 0.35f; // oscilador desligado: apagado, como a onda

    const auto area = drawGraphBackground (g, *this);

    // Faixas, de cima para baixo: título, barras, cents do eixo, régua L/R.
    // Muito baixo: só barras e régua.
    const bool roomForText = area.getHeight() >= 80.0f && area.getWidth() >= 120.0f;
    auto rest = area;
    const auto titleRow = roomForText ? rest.removeFromTop (16.0f) : juce::Rectangle<float>();
    const auto panRow   = rest.removeFromBottom (16.0f);
    rest.removeFromBottom (4.0f);
    const auto axisRow  = roomForText ? rest.removeFromBottom (14.0f) : juce::Rectangle<float>();
    const auto bars     = rest.reduced (0.0f, 2.0f);

    // Eixo de afinação: −100 a +100 cents (a faixa do Detune) em volta da nota. Margem nas
    // pontas para os textos e as letras L/R.
    constexpr float axisCents = 100.0f;
    constexpr float sideMargin = 14.0f;
    const float halfWidth = area.getWidth() / 2.0f - sideMargin;
    const float centreX = area.getCentreX();
    auto xOfCents = [centreX, halfWidth] (double cents)
    {
        return centreX + static_cast<float> (cents / axisCents) * halfWidth;
    };

    g.setFont (juce::FontOptions (11.0f));

    // Grade: ±50 ¢ e ±100 ¢ bem fracas; a nota (0 ¢) mais forte.
    g.setColour (gridColour (*this));
    for (const float c : { -100.0f, -50.0f, 50.0f, 100.0f })
        g.drawVerticalLine (juce::roundToInt (xOfCents (c)), bars.getY(), bars.getBottom());
    g.setColour (gridColour (*this).withMultipliedAlpha (2.0f));
    g.drawVerticalLine (juce::roundToInt (centreX), bars.getY(), bars.getBottom());

    // Linhas da escala atual (steps vizinhos), tracejadas em bronze.
    const float dashes[] = { 3.0f, 3.0f };
    g.setColour (Palette::secondary.withAlpha (0.8f * alpha));
    for (int i = 0; i < drawnLines.count; ++i)
    {
        const float x = xOfCents (drawnLines.cents[static_cast<std::size_t> (i)]);
        g.drawDashedLine ({ x, bars.getY(), x, bars.getBottom() }, dashes, 2, 1.5f);
    }

    // Barras: posição = desvio da cópia; altura = volume relativo (centrais 1, laterais Blend).
    const float barWidth = juce::jlimit (3.0f, 8.0f, area.getWidth() / 50.0f);
    for (int k = 0; k < layout.count; ++k)
    {
        const float level = Unison::relativeLevel (k, layout.count, blend);
        const float x = xOfCents (layout.detuneCents[static_cast<std::size_t> (k)]);
        const float h = juce::jmax (2.0f, level * bars.getHeight());
        const auto bar = juce::Rectangle<float> (x - barWidth / 2.0f, bars.getBottom() - h, barWidth, h);

        g.setColour (curveColour (*this).withAlpha (alpha * (level > 0.0f ? 0.85f : 0.3f)));
        g.fillRoundedRectangle (bar, juce::jmin (2.0f, barWidth / 2.0f));
    }

    // Régua L—C—R do Width, com um tique por cópia na posição do pan.
    const float rulerY = panRow.getCentreY();
    g.setColour (gridTextColour (*this));
    g.drawHorizontalLine (juce::roundToInt (rulerY), centreX - halfWidth, centreX + halfWidth);
    g.drawVerticalLine (juce::roundToInt (centreX), rulerY - 3.0f, rulerY + 3.0f);
    g.drawText ("L", juce::Rectangle<float> (area.getX(), panRow.getY(), sideMargin - 2.0f, panRow.getHeight()),
                juce::Justification::centredLeft);
    g.drawText ("R", juce::Rectangle<float> (area.getRight() - sideMargin + 2.0f, panRow.getY(), sideMargin - 2.0f, panRow.getHeight()),
                juce::Justification::centredRight);

    g.setColour (curveColour (*this).withAlpha (alpha));
    for (int k = 0; k < layout.count; ++k)
    {
        const float x = centreX + Unison::pan (k, layout.count, width) * halfWidth;
        g.fillRoundedRectangle (juce::Rectangle<float> (x - 1.5f, rulerY - 5.0f, 3.0f, 10.0f), 1.0f);
    }

    if (! roomForText)
        return;

    // Cents do eixo, embaixo das barras.
    g.setColour (gridTextColour (*this));
    const juce::String minus (juce::CharPointer_UTF8 ("\xe2\x88\x92")); // −
    for (const auto& [cents, text] : { std::pair { -100.0f, minus + "100" }, std::pair { -50.0f, minus + "50" },
                                       std::pair { 0.0f, juce::String ("0 ct") }, std::pair { 50.0f, juce::String ("+50") },
                                       std::pair { 100.0f, juce::String ("+100") } })
        g.drawText (text, juce::Rectangle<float> (xOfCents (cents) - 22.0f, axisRow.getY(), 44.0f, axisRow.getHeight()),
                    juce::Justification::centred);

    // Título: "4 voices · ±50 ct · blend 78 % · width 100 %"
    const juce::String dot (juce::CharPointer_UTF8 ("\xc2\xb7"));        // ·
    const juce::String plusMinus (juce::CharPointer_UTF8 ("\xc2\xb1"));  // ±
    const juce::String title = juce::String (layout.count) + (layout.count == 1 ? " voice" : " voices")
                             + " " + dot + " " + plusMinus + juce::String (detune, 1) + " ct"
                             + " " + dot + " blend " + juce::String (juce::roundToInt (blend * 100.0f)) + " %"
                             + " " + dot + " width " + juce::String (juce::roundToInt (width * 100.0f)) + " %";
    g.setColour (gridTextColour (*this).withMultipliedAlpha (1.6f));
    g.drawFittedText (title, titleRow.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
}

// ----------------------------------------------------------------------------- HarmonicsGraph

HarmonicsGraph::HarmonicsGraph (std::function<HarmonicsView::Snapshot()> snapshotSource)
    : source (std::move (snapshotSource))
{
    if (source != nullptr)
        drawn = source();
    startTimerHz (refreshHz);
}

void HarmonicsGraph::timerCallback()
{
    if (source == nullptr)
        return;

    const auto now = source();
    if (! (now == drawn))
    {
        drawn = now;
        repaint();
    }
}

void HarmonicsGraph::paint (juce::Graphics& g)
{
    const auto area = drawGraphBackground (g, *this);
    const bool roomForText = area.getHeight() >= 80.0f && area.getWidth() >= 140.0f;

    // Faixas: título em cima, marcas 1×, 2×, 4×… embaixo, barras no meio.
    auto rest = area;
    const auto titleRow = roomForText ? rest.removeFromTop (16.0f) : juce::Rectangle<float>();
    const auto axisRow  = roomForText ? rest.removeFromBottom (14.0f) : juce::Rectangle<float>();
    const auto bars     = rest.reduced (0.0f, 2.0f);

    constexpr float sideMargin = 10.0f;
    const float left  = area.getX() + sideMargin;
    const float width = area.getWidth() - 2.0f * sideMargin;
    auto xOfCents = [left, width] (double cents)
    {
        return left + static_cast<float> (cents / HarmonicsView::maxCents) * width;
    };

    g.setFont (juce::FontOptions (11.0f));

    // Oitavas da fundamental (1×, 2×, 4×, 8×, 16×): referência fixa do eixo.
    g.setColour (gridColour (*this));
    for (int octave = 0; octave <= 4; ++octave)
        g.drawVerticalLine (juce::roundToInt (xOfCents (1200.0 * octave)), bars.getY(), bars.getBottom());

    // Alturas da escala acima da nota, em bronze. Escalas densas (ex.: 72 EDO) ficam mais fracas
    // para não virar um borrão.
    if (drawn.lineCount > 0)
    {
        const float spacing = width / static_cast<float> (drawn.lineCount);
        g.setColour (Palette::secondary.withAlpha (juce::jlimit (0.15f, 0.6f, spacing / 12.0f)));
        for (int i = 0; i < drawn.lineCount; ++i)
        {
            const float x = xOfCents (drawn.lineCents[static_cast<std::size_t> (i)]);
            g.drawLine (x, bars.getY(), x, bars.getBottom(), 1.0f);
        }
    }

    // Harmônicos: altura = volume em dB (de −48 dB até o mais forte da forma).
    constexpr double floorDb = -48.0;
    double loudest = 0.0;
    for (int h = 1; h <= AlignedSpectrum::numPartials; ++h)
        loudest = std::max (loudest, HarmonicsView::amplitude (drawn.recipe, h));

    const float barWidth = juce::jlimit (2.0f, 5.0f, width / 120.0f);
    for (int h = 1; h <= AlignedSpectrum::numPartials && loudest > 0.0; ++h)
    {
        const double db = 20.0 * std::log10 (HarmonicsView::amplitude (drawn.recipe, h) / loudest + 1e-12);
        if (db < floorDb)
            continue; // harmônico ausente na forma (ex.: os pares da quadrada)

        const float barHeight = juce::jmax (3.0f, static_cast<float> ((db - floorDb) / -floorDb) * bars.getHeight());
        const double ratio = drawn.ratios.ratio[static_cast<std::size_t> (h - 1)];
        const float xNatural = xOfCents (1200.0 * std::log2 (static_cast<double> (h)));
        const float xNow = xOfCents (1200.0 * std::log2 (ratio));
        const bool aligned = h <= drawn.ratios.alignedCount;

        // Posição natural: risco fino e fraco, só quando a barra saiu de lá.
        if (std::abs (xNow - xNatural) > 1.0f)
        {
            g.setColour (gridTextColour (*this));
            g.fillRect (juce::Rectangle<float> (xNatural - 0.5f, bars.getBottom() - barHeight, 1.0f, barHeight));
        }

        // Harmônico acima do limite de colisão: não alinha (fica natural), cor apagada.
        g.setColour (aligned ? curveColour (*this).withAlpha (0.9f) : gridTextColour (*this));
        g.fillRoundedRectangle (juce::Rectangle<float> (xNow - barWidth / 2.0f, bars.getBottom() - barHeight, barWidth, barHeight),
                                juce::jmin (2.0f, barWidth / 2.0f));
    }

    if (! roomForText)
        return;

    g.setColour (gridTextColour (*this));
    const juce::String times (juce::CharPointer_UTF8 ("\xc3\x97")); // ×
    for (int octave = 0; octave <= 4; ++octave)
        g.drawText (juce::String (1 << octave) + times,
                    juce::Rectangle<float> (xOfCents (1200.0 * octave) - 16.0f, axisRow.getY(), 32.0f, axisRow.getHeight()),
                    juce::Justification::centred);

    // Título: "Osc 1 · Saw · note 60 · 22 of 24 aligned"
    static const juce::StringArray waveNames { "Sine", "Saw", "Square", "Triangle", "Custom" };
    const juce::String dot (juce::CharPointer_UTF8 (" \xc2\xb7 ")); // ·
    juce::String title = "Osc " + juce::String (drawn.oscIndex + 1) + dot
                       + waveNames[juce::jlimit (0, waveNames.size() - 1, drawn.waveform)] + dot
                       + (drawn.noteHeld ? "note " : "ref ") + juce::String (drawn.note);
    if (drawn.master)
        title += dot + "MTS-ESP";
    title += dot + (drawn.align > 0.0
                        ? juce::String (drawn.ratios.alignedCount) + " of " + juce::String (AlignedSpectrum::numPartials) + " aligned"
                        : juce::String ("natural"));

    g.setColour (gridTextColour (*this).withMultipliedAlpha (1.6f));
    g.drawFittedText (title, titleRow.toNearestInt(), juce::Justification::centredLeft, 1, 0.8f);
}

// ----------------------------------------------------------------------------- EnvelopeGraph

EnvelopeGraph::EnvelopeGraph (juce::AudioProcessorValueTreeState& apvts, ParamIDs4 ids,
                              const PluginProcessor::EnvelopeDisplay& display)
    : envelopeDisplay (display)
{
    attackParam  = apvts.getRawParameterValue (ids.attack);
    decayParam   = apvts.getRawParameterValue (ids.decay);
    sustainParam = apvts.getRawParameterValue (ids.sustain);
    releaseParam = apvts.getRawParameterValue (ids.release);
    startTimerHz (refreshHz);
}

void EnvelopeGraph::timerCallback()
{
    const bool paramsChanged = attackParam->load() != drawnAttack || decayParam->load() != drawnDecay
                            || sustainParam->load() != drawnSustain || releaseParam->load() != drawnRelease;

    // Enquanto uma nota toca, o ponto anda: redesenha sempre. Redesenha mais uma vez quando
    // a nota acaba, para apagar o ponto.
    const bool dotVisible = envelopeDisplay.stage.load() != static_cast<int> (GraphMath::EnvelopeStage::idle);

    if (paramsChanged || dotVisible || dotWasVisible)
        repaint();
}

void EnvelopeGraph::paint (juce::Graphics& g)
{
    drawnAttack  = attackParam->load();
    drawnDecay   = decayParam->load();
    drawnSustain = sustainParam->load();
    drawnRelease = releaseParam->load();

    const double a = drawnAttack / 1000.0, d = drawnDecay / 1000.0, r = drawnRelease / 1000.0; // ms -> s
    const auto layout = GraphMath::layoutEnvelope (a, d, r);

    const auto area = drawGraphBackground (g, *this).reduced (2.0f, 4.0f);
    auto toPoint = [&area] (double x, double level)
    {
        return juce::Point<float> (area.getX() + static_cast<float> (x) * area.getWidth(),
                                   area.getBottom() - static_cast<float> (level) * area.getHeight());
    };

    // Linhas verticais separando as fases.
    g.setColour (gridColour (*this));
    for (double x : { layout.attackEnd, layout.decayEnd, layout.sustainEnd })
        g.drawVerticalLine (juce::roundToInt (toPoint (x, 0.0).x), area.getY(), area.getBottom());

    // O juce::ADSR é feito de retas: attack sobe até 1, decay desce até o sustain,
    // o sustain fica parado e o release desce até 0.
    juce::Path shape;
    shape.startNewSubPath (toPoint (0.0, 0.0));
    shape.lineTo (toPoint (layout.attackEnd, 1.0));
    shape.lineTo (toPoint (layout.decayEnd, drawnSustain));
    shape.lineTo (toPoint (layout.sustainEnd, drawnSustain));
    shape.lineTo (toPoint (1.0, 0.0));

    juce::Path fill (shape);
    fill.closeSubPath();
    g.setColour (curveColour (*this).withAlpha (0.2f));
    g.fillPath (fill);

    g.setColour (curveColour (*this));
    g.strokePath (shape, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));

    // Ponto da nota que está tocando: x pela fase e pelo tempo, y pelo nível real do envelope.
    const auto stage = static_cast<GraphMath::EnvelopeStage> (envelopeDisplay.stage.load());
    dotWasVisible = stage != GraphMath::EnvelopeStage::idle;

    if (dotWasVisible)
    {
        const double x = GraphMath::envelopePlayheadX (layout, stage, envelopeDisplay.secondsInStage.load(), a, d, r);
        const auto dot = toPoint (x, envelopeDisplay.level.load());

        g.setColour (Palette::text);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (dot));
    }
}
