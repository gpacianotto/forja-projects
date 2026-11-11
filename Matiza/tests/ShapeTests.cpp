// Nomes de TEST_CASE sem acento: o ctest no Windows não os encontra se tiverem.
// Testes do RF15: pontos desenhados, buffer triplo e mipmaps de formas desenhadas.
#include "CustomShapes.h"
#include "Lfo.h"
#include "ShapePoints.h"
#include "TripleBuffer.h"
#include "Wavetables.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>
#include <vector>

using Catch::Matchers::WithinAbs;
using Preset = ShapePoints::Preset;

TEST_CASE ("Pontos: formas prontas passam pelos valores esperados")
{
    const auto saw = ShapePoints::preset (Preset::sawUp);
    CHECK_THAT (saw.evaluate (0.0), WithinAbs (-1.0, 1e-6));
    CHECK_THAT (saw.evaluate (0.5), WithinAbs (0.0, 1e-6));
    CHECK_THAT (saw.evaluate (1.0), WithinAbs (1.0, 1e-6));

    const auto square = ShapePoints::preset (Preset::square);
    CHECK_THAT (square.evaluate (0.25), WithinAbs (1.0, 1e-6));
    CHECK_THAT (square.evaluate (0.75), WithinAbs (-1.0, 1e-6));

    const auto triUp = ShapePoints::preset (Preset::triangleUp);
    CHECK_THAT (triUp.evaluate (0.25), WithinAbs (1.0, 1e-6));
    CHECK_THAT (triUp.evaluate (0.75), WithinAbs (-1.0, 1e-6));

    // O seno é aproximado por curvas: perto da senoide de verdade.
    const auto sine = ShapePoints::preset (Preset::sine);
    for (double x = 0.0; x <= 1.0; x += 0.01)
        CHECK_THAT (sine.evaluate (x), WithinAbs (std::sin (2.0 * std::numbers::pi * x), 0.08));
}

TEST_CASE ("Pontos: curva passa pelas pontas e muda o meio")
{
    CHECK_THAT (ShapePoints::curveT (0.0, 0.7f), WithinAbs (0.0, 1e-9));
    CHECK_THAT (ShapePoints::curveT (1.0, 0.7f), WithinAbs (1.0, 1e-9));
    CHECK (ShapePoints::curveT (0.5, 0.7f) < 0.5);  // começa devagar
    CHECK (ShapePoints::curveT (0.5, -0.7f) > 0.5); // começa rápido
    CHECK (ShapePoints::curveT (0.5, 0.0f) == 0.5);
}

TEST_CASE ("Pontos: adicionar, mover e apagar respeitam as regras")
{
    auto shape = ShapePoints::preset (Preset::flat);
    REQUIRE (shape.size() == 2);

    const int index = shape.add (0.5f, 0.8f);
    CHECK (index == 1);
    CHECK (shape.size() == 3);
    CHECK_THAT (shape.evaluate (0.5), WithinAbs (0.8, 1e-6));

    // x fica entre os vizinhos; y fica em -1..1.
    shape.move (1, 2.0f, 5.0f);
    CHECK (shape[1].x == 1.0f);
    CHECK (shape[1].y == 1.0f);

    // As pontas só mudam de altura.
    shape.move (0, 0.3f, -0.5f);
    CHECK (shape[0].x == 0.0f);
    CHECK (shape[0].y == -0.5f);

    CHECK_FALSE (shape.remove (0));
    CHECK_FALSE (shape.remove (shape.size() - 1));
    CHECK (shape.remove (1));
    CHECK (shape.size() == 2);
}

TEST_CASE ("Pontos: no maximo 32")
{
    auto shape = ShapePoints::preset (Preset::flat);
    for (int i = 0; i < 40; ++i)
        shape.add (static_cast<float> (i) / 40.0f, 0.0f);

    CHECK (shape.size() == ShapePoints::maxPoints);
    CHECK (shape.add (0.5f, 0.0f) == -1);
}

TEST_CASE ("Pontos: texto do estado vai e volta igual; texto invalido e recusado")
{
    auto shape = ShapePoints::preset (Preset::sine);
    shape.add (0.6f, 0.3f);
    shape.setCurve (2, 0.25f);

    const auto restored = ShapePoints::fromString (shape.toString());
    REQUIRE (restored.has_value());
    CHECK (*restored == shape);

    CHECK_FALSE (ShapePoints::fromString ("").has_value());
    CHECK_FALSE (ShapePoints::fromString ("abc").has_value());
    CHECK_FALSE (ShapePoints::fromString ("0,0,0").has_value()); // um ponto só
}

TEST_CASE ("Buffer triplo: o leitor sempre recebe a ultima publicada e o escritor nunca a usa")
{
    TripleBuffer<int> buffer;
    buffer.fillAll ([] (int& v) { v = 0; });

    const int* reading = &buffer.read();

    for (int value = 1; value <= 10; ++value)
    {
        int& slot = buffer.beginWrite();
        CHECK (&slot != reading); // não escreve no que o leitor está usando
        slot = value;
        buffer.publish();

        reading = &buffer.read();
        CHECK (*reading == value);
    }
}

TEST_CASE ("Mipmaps desenhados: serra desenhada vira quase a serra pronta, sem DC")
{
    std::vector<float> cycle (Wavetables::tableSize);
    const auto saw = ShapePoints::preset (Preset::sawUp);
    for (int i = 0; i < Wavetables::tableSize; ++i)
        cycle[static_cast<size_t> (i)] = saw.evaluate (static_cast<double> (i) / Wavetables::tableSize);

    std::vector<float> levels (Wavetables::mipmapSetSize);
    Wavetables::buildMipmaps (cycle.data(), levels.data());

    // Longe do salto, o nível 0 fica perto da serra pronta.
    const float* builtin = Wavetables::table (Waveform::saw, 0);
    for (int i = 256; i < Wavetables::tableSize - 256; i += 128)
        CHECK_THAT (levels[static_cast<size_t> (i)], WithinAbs (builtin[i], 0.02));

    // Sem DC em nenhum nível, e a amostra extra repete a primeira.
    for (int level = 0; level < Wavetables::numLevels; ++level)
    {
        const float* t = levels.data() + level * Wavetables::levelStride;
        double sum = 0.0;
        for (int i = 0; i < Wavetables::tableSize; ++i)
            sum += t[i];
        CHECK_THAT (sum / Wavetables::tableSize, WithinAbs (0.0, 1e-4));
        CHECK (t[Wavetables::tableSize] == t[0]);
    }
}

TEST_CASE ("Mipmaps desenhados: forma deslocada para cima perde a media (DC)")
{
    std::vector<float> cycle (Wavetables::tableSize, 0.5f); // linha reta em 0,5
    std::vector<float> levels (Wavetables::mipmapSetSize);
    Wavetables::buildMipmaps (cycle.data(), levels.data());

    for (int i = 0; i < Wavetables::tableSize; i += 256)
        CHECK_THAT (levels[static_cast<size_t> (i)], WithinAbs (0.0, 1e-5));
}

TEST_CASE ("CustomShapes: mudar os pontos chega na tabela do audio")
{
    auto shapes = std::make_unique<CustomShapes>(); // ~1,6 MB: no heap

    auto ramp = ShapePoints::preset (Preset::sawUp);
    shapes->setLfoPoints (1, ramp);

    const float* table = shapes->readLfoTable (1);
    CHECK_THAT (Lfo::customValue (table, 0.0),  WithinAbs (-1.0, 1e-5));
    CHECK_THAT (Lfo::customValue (table, 0.5),  WithinAbs (0.0, 1e-3));
    CHECK_THAT (Lfo::customValue (table, 0.99), WithinAbs (0.98, 1e-3));

    // O LFO 0 não mudou: continua com o seno inicial.
    CHECK_THAT (Lfo::customValue (shapes->readLfoTable (0), 0.25), WithinAbs (1.0, 1e-5));
}
