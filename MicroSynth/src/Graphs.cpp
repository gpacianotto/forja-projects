#include "Graphs.h"
#include "GraphMath.h"
#include "Oscillator.h"
#include "Parameters.h"

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

    // Cor de destaque das curvas (ciano), bem visível sobre o fundo escuro do tema Midnight.
    juce::Colour curveColour (const juce::Component&)
    {
        return juce::Colour (0xff5ec8e5);
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

FilterGraph::FilterGraph (juce::AudioProcessorValueTreeState& apvts)
{
    cutoffParam = apvts.getRawParameterValue (ParamIDs::cutoff);
    resoParam   = apvts.getRawParameterValue (ParamIDs::reso);
    startTimerHz (refreshHz);
}

void FilterGraph::timerCallback()
{
    if (cutoffParam->load() != drawnCutoff || resoParam->load() != drawnReso)
        repaint();
}

void FilterGraph::paint (juce::Graphics& g)
{
    drawnCutoff = cutoffParam->load();
    drawnReso   = resoParam->load();

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
    juce::Path curve;
    bool firstPoint = true;
    for (float x = area.getX(); x <= area.getRight(); x += 2.0f)
    {
        const double db = GraphMath::lowpassMagnitudeDb (xToHz (x, area), drawnCutoff, drawnReso);
        const float y = dbToY (db, area);

        if (firstPoint)
            curve.startNewSubPath (x, y);
        else
            curve.lineTo (x, y);

        firstPoint = false;
    }

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

OscillatorGraph::OscillatorGraph (juce::AudioProcessorValueTreeState& apvts, std::size_t oscIndex)
{
    onParam    = apvts.getRawParameterValue (ParamIDs::oscOn[oscIndex]);
    waveParam  = apvts.getRawParameterValue (ParamIDs::oscWave[oscIndex]);
    levelParam = apvts.getRawParameterValue (ParamIDs::oscLevel[oscIndex]);
    startTimerHz (refreshHz);
}

void OscillatorGraph::timerCallback()
{
    if (onParam->load() != drawnOn || waveParam->load() != drawnWave || levelParam->load() != drawnLevel)
        repaint();
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
    Oscillator oscillator;
    oscillator.setWaveform (static_cast<Waveform> (juce::roundToInt (drawnWave)));
    const int numPoints = juce::jmax (2, juce::roundToInt (area.getWidth()));
    oscillator.setFrequency (1.0, numPoints);

    const float amplitude = juce::jlimit (0.0f, 1.0f, drawnLevel / 100.0f) * area.getHeight() / 2.0f;

    juce::Path wave;
    for (int i = 0; i <= numPoints; ++i)
    {
        const float x = area.getX() + area.getWidth() * static_cast<float> (i) / static_cast<float> (numPoints);
        const float y = area.getCentreY() - oscillator.nextSample() * amplitude;

        if (i == 0)
            wave.startNewSubPath (x, y);
        else
            wave.lineTo (x, y);
    }

    const bool enabled = drawnOn > 0.5f;
    g.setColour (curveColour (*this).withAlpha (enabled ? 1.0f : 0.3f));
    g.strokePath (wave, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
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

        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre (dot));
    }
}
