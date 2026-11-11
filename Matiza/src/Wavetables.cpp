#include "Wavetables.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace Wavetables
{
    namespace
    {
        constexpr int samplesPerTable = tableSize + 1;
        constexpr double pi = std::numbers::pi;

        // Amplitude do harmônico h (coeficiente da série de Fourier em senos) de cada forma,
        // escolhida para a forma e a fase ficarem iguais às do oscilador PolyBLEP antigo.
        double harmonicAmplitude (Waveform waveform, int h)
        {
            switch (waveform)
            {
                case Waveform::sine:
                    return h == 1 ? 1.0 : 0.0;

                case Waveform::saw: // rampa de -1 a +1, salto para baixo em t = 0
                    return -2.0 / (pi * h);

                case Waveform::square: // +1 na primeira metade, -1 na segunda (só harmônicos ímpares)
                    return h % 2 == 1 ? 4.0 / (pi * h) : 0.0;

                case Waveform::triangle: // começa em 0 e desce (harmônicos ímpares, caem com 1/h²)
                {
                    if (h % 2 == 0)
                        return 0.0;
                    const double sign = ((h - 1) / 2) % 2 == 0 ? 1.0 : -1.0;
                    return -sign * 8.0 / (pi * pi * h * h);
                }
            }
            return 0.0;
        }

        // Todas as tabelas num único vetor: [forma][nível][amostra].
        class Bank
        {
        public:
            Bank() : samples (static_cast<size_t> (numWaveforms * numLevels * samplesPerTable))
            {
                // Seno de 1 ciclo amostrado uma vez. O harmônico h na amostra i é
                // sin(2π·h·i/N) = sineTable[(h·i) mod N]: sem chamar std::sin no laço.
                std::vector<double> sineTable (tableSize);
                for (int i = 0; i < tableSize; ++i)
                    sineTable[static_cast<size_t> (i)] = std::sin (2.0 * pi * i / tableSize);

                std::vector<double> sum (tableSize);

                for (int w = 0; w < numWaveforms; ++w)
                {
                    const auto waveform = static_cast<Waveform> (w);

                    for (int level = 0; level < numLevels; ++level)
                    {
                        std::fill (sum.begin(), sum.end(), 0.0);
                        const int harmonics = maxHarmonic (level);

                        for (int h = 1; h <= harmonics; ++h)
                        {
                            const double amplitude = harmonicAmplitude (waveform, h);
                            if (amplitude == 0.0)
                                continue;

                            for (int i = 0; i < tableSize; ++i)
                                sum[static_cast<size_t> (i)] += amplitude * sineTable[static_cast<size_t> ((h * i) & (tableSize - 1))];
                        }

                        float* out = data (waveform, level);
                        for (int i = 0; i < tableSize; ++i)
                            out[i] = static_cast<float> (sum[static_cast<size_t> (i)]);
                        out[tableSize] = out[0];
                    }
                }
            }

            float* data (Waveform waveform, int level) noexcept
            {
                const auto index = (static_cast<int> (waveform) * numLevels + level) * samplesPerTable;
                return samples.data() + index;
            }

        private:
            std::vector<float> samples;
        };

        Bank& bank()
        {
            // "static" local: construído na primeira chamada, uma vez só (como um singleton
            // preguiçoso em Java, mas com a sincronização garantida pela linguagem).
            static Bank instance;
            return instance;
        }
    }

    int maxHarmonic (int level) noexcept
    {
        level = std::clamp (level, 0, numLevels - 1);
        // 1024 / 2^(nível/2): cai pela metade a cada oitava (dois níveis).
        const int harmonics = static_cast<int> (std::floor (1024.0 / std::pow (2.0, level / 2.0)));
        return std::clamp (harmonics, 1, tableSize / 2 - 1);
    }

    int levelFor (double increment) noexcept
    {
        if (! (increment > 0.0)) // também pega NaN
            return 0;

        // Estimativa direta (resolvendo 1024 / 2^(k/2) × increment <= 0,5) e um ajuste fino,
        // porque maxHarmonic arredonda para baixo.
        int level = static_cast<int> (std::ceil (2.0 * std::log2 (2048.0 * increment)));
        level = std::clamp (level, 0, numLevels - 1);

        while (level > 0 && maxHarmonic (level - 1) * increment <= 0.5)
            --level;
        while (level < numLevels - 1 && maxHarmonic (level) * increment > 0.5)
            ++level;

        return level;
    }

    const float* table (Waveform waveform, int level) noexcept
    {
        return bank().data (waveform, std::clamp (level, 0, numLevels - 1));
    }
}
