#include "Wavetables.h"

#include <algorithm>
#include <cmath>
#include <complex>
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

                case Waveform::custom: // não fica no banco
                    return 0.0;
            }
            return 0.0;
        }

        // Todas as tabelas num único vetor: [forma][nível][amostra].
        class Bank
        {
        public:
            Bank() : samples (static_cast<size_t> (numBuiltinWaveforms * numLevels * samplesPerTable))
            {
                // Seno de 1 ciclo amostrado uma vez. O harmônico h na amostra i é
                // sin(2π·h·i/N) = sineTable[(h·i) mod N]: sem chamar std::sin no laço.
                std::vector<double> sineTable (tableSize);
                for (int i = 0; i < tableSize; ++i)
                    sineTable[static_cast<size_t> (i)] = std::sin (2.0 * pi * i / tableSize);

                std::vector<double> sum (tableSize);

                for (int w = 0; w < numBuiltinWaveforms; ++w)
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

        // FFT complexa in-place (radix-2, Cooley-Tukey). n precisa ser potência de 2.
        // inverse = true faz a transformada inversa (sem dividir por n).
        void fft (std::vector<std::complex<double>>& a, bool inverse)
        {
            const std::size_t n = a.size();

            // Reordena pelos bits invertidos do índice.
            for (std::size_t i = 1, j = 0; i < n; ++i)
            {
                std::size_t bit = n >> 1;
                for (; j & bit; bit >>= 1)
                    j ^= bit;
                j ^= bit;
                if (i < j)
                    std::swap (a[i], a[j]);
            }

            // Junta transformadas de tamanho 2, 4, 8... (as "borboletas").
            for (std::size_t length = 2; length <= n; length <<= 1)
            {
                const double angle = (inverse ? 2.0 : -2.0) * pi / static_cast<double> (length);
                const std::complex<double> step (std::cos (angle), std::sin (angle));

                for (std::size_t start = 0; start < n; start += length)
                {
                    std::complex<double> w (1.0);
                    for (std::size_t k = 0; k < length / 2; ++k)
                    {
                        const auto even = a[start + k];
                        const auto odd  = a[start + k + length / 2] * w;
                        a[start + k]              = even + odd;
                        a[start + k + length / 2] = even - odd;
                        w *= step;
                    }
                }
            }
        }

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
        if (waveform == Waveform::custom)
            waveform = Waveform::saw;

        return bank().data (waveform, std::clamp (level, 0, numLevels - 1));
    }

    void buildMipmaps (const float* cycle, float* out)
    {
        // Espectro do ciclo desenhado: o índice h é o harmônico h.
        std::vector<std::complex<double>> spectrum (tableSize);
        for (int i = 0; i < tableSize; ++i)
            spectrum[static_cast<std::size_t> (i)] = cycle[i];
        fft (spectrum, false);

        std::vector<std::complex<double>> work (tableSize);

        for (int level = 0; level < numLevels; ++level)
        {
            const int harmonics = maxHarmonic (level);

            // Mantém só os harmônicos 1..harmonics (e os espelhos, N-h, que um sinal real tem).
            // O DC (h = 0, a média) sai: uma onda desenhada fora do centro não empurra a saída.
            std::fill (work.begin(), work.end(), std::complex<double> {});
            for (int h = 1; h <= harmonics; ++h)
            {
                work[static_cast<std::size_t> (h)] = spectrum[static_cast<std::size_t> (h)];
                work[static_cast<std::size_t> (tableSize - h)] = spectrum[static_cast<std::size_t> (tableSize - h)];
            }

            fft (work, true);

            float* levelOut = out + level * levelStride;
            for (int i = 0; i < tableSize; ++i)
                levelOut[i] = static_cast<float> (work[static_cast<std::size_t> (i)].real() / tableSize);
            levelOut[tableSize] = levelOut[0];
        }
    }
}
