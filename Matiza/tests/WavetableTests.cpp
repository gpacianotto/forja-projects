// Nomes de TEST_CASE sem acento: o ctest no Windows não os encontra se tiverem.
#include "Wavetables.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <numbers>

using Catch::Matchers::WithinAbs;
using namespace Wavetables;

TEST_CASE ("Mipmaps: harmonicos caem pela metade a cada oitava (dois niveis)")
{
    CHECK (maxHarmonic (0) == 1023);
    CHECK (maxHarmonic (2) == 512);
    CHECK (maxHarmonic (4) == 256);
    CHECK (maxHarmonic (numLevels - 1) == 1);

    for (int level = 1; level < numLevels; ++level)
        CHECK (maxHarmonic (level) <= maxHarmonic (level - 1));
}

TEST_CASE ("Mipmaps: o nivel escolhido nunca passa de Nyquist e e o mais rico possivel")
{
    // De 20 Hz a 20 kHz a 44,1 kHz e a 96 kHz.
    for (double sampleRate : { 44100.0, 96000.0 })
    {
        for (double hz = 20.0; hz < 20000.0; hz *= 1.07)
        {
            const double increment = hz / sampleRate;
            const int level = levelFor (increment);
            INFO ("hz = " << hz << ", sr = " << sampleRate << ", nivel = " << level);

            CHECK (maxHarmonic (level) * increment <= 0.5);                    // sem aliasing
            if (level > 0)
                CHECK (maxHarmonic (level - 1) * increment > 0.5);             // o anterior teria
        }
    }
}

TEST_CASE ("Mipmaps: incremento invalido usa o nivel 0 sem quebrar")
{
    CHECK (levelFor (0.0) == 0);
    CHECK (levelFor (-1.0) == 0);
    CHECK (levelFor (std::nan ("")) == 0);
    CHECK (levelFor (10.0) == numLevels - 1);
}

TEST_CASE ("Ultimo nivel da serra e uma senoide pura (so o 1o harmonico)")
{
    const float* saw = table (Waveform::saw, numLevels - 1);
    const double amplitude = -2.0 / std::numbers::pi;

    for (int i = 0; i < tableSize; i += 64)
        CHECK_THAT (saw[i], WithinAbs (amplitude * std::sin (2.0 * std::numbers::pi * i / tableSize), 1e-5));
}

TEST_CASE ("Tabelas tem a amostra extra igual a primeira (para interpolar na volta)")
{
    for (auto wave : { Waveform::sine, Waveform::saw, Waveform::square, Waveform::triangle })
        for (int level = 0; level < numLevels; ++level)
            CHECK (table (wave, level)[tableSize] == table (wave, level)[0]);
}
