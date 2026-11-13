#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <sstream>
#include <string>

// Afinação desenhada (RF17): a posição em cents de cada step dentro da oitava, guardada junto
// com o número de steps (N) para o qual foi desenhada. C++ puro, sem JUCE: testável sozinha.
//
// Regra do Spec: a tabela só vale quando o N dela é igual ao `edo` atual. Assim, mudar o EDO
// por qualquer caminho (knob, automação, preset) volta à afinação igual, sem lock nenhum.
//
// Invariantes (garantidas por moveStep e fromString):
//   cents[0] = 0 (o step 0 é a Ref Note, fixo);
//   cents[0] < cents[1] < ... < cents[N-1] < 1200, com pelo menos minGapCents entre vizinhas.
//
// É um tipo de valor pequeno (~600 bytes) e sem ponteiros: copiar é seguro e não aloca, então
// a voz pode copiar a tabela inteira no note-on (RF3), como já copia o resto da afinação.
struct TuningTable
{
    static constexpr int    maxEdo         = 72;   // máximo do parâmetro edo
    static constexpr double centsPerOctave = 1200.0;
    static constexpr double minGapCents    = 1.0;  // espaço mínimo entre duas linhas

    int edo = 0; // 0 = nenhuma tabela desenhada (vale a afinação igual)
    std::array<double, maxEdo> cents {};

    bool operator== (const TuningTable&) const = default;

    // Tabela equidistante (afinação igual) de N steps.
    static TuningTable equal (int edo) noexcept
    {
        TuningTable table;
        table.edo = std::clamp (edo, 1, maxEdo);
        for (int k = 0; k < table.edo; ++k)
            table.cents[static_cast<std::size_t> (k)] = centsPerOctave * k / table.edo;
        return table;
    }

    // A tabela vale para este EDO?
    bool isFor (int currentEdo) const noexcept { return edo >= 1 && edo == currentEdo; }

    // Algum step fora da posição igual? (para mostrar "custom" e habilitar o Reset)
    bool isCustomFor (int currentEdo) const noexcept
    {
        if (! isFor (currentEdo))
            return false;

        for (int k = 0; k < edo; ++k)
            if (std::abs (cents[static_cast<std::size_t> (k)] - centsPerOctave * k / edo) > 1.0e-6)
                return true;

        return false;
    }

    // Posição do step k (0..edo-1) no EDO atual: a da tabela, se ela vale, senão a igual.
    double stepCents (int step, int currentEdo) const noexcept
    {
        const int n = std::max (1, currentEdo);
        return isFor (n) ? cents[static_cast<std::size_t> (step)] : centsPerOctave * step / n;
    }

    // Altura em cents (a partir da Ref Note) de uma altura medida em steps, que pode ser
    // negativa, passar da oitava e ser fracionária (glide, pitch bend). Steps inteiros caem
    // exatamente nas linhas; entre dois steps, interpola em linha reta em cents (RF17).
    double stepsToCents (double steps, int currentEdo) const noexcept
    {
        const int n = std::max (1, currentEdo);
        if (! isFor (n))
            return centsPerOctave * steps / n; // afinação igual: conta direta, como antes

        const double lower = std::floor (steps);
        const double frac  = steps - lower;
        const int    i     = static_cast<int> (lower);

        const double a = wholeStepCents (i, n);
        return frac == 0.0 ? a : a + (wholeStepCents (i + 1, n) - a) * frac;
    }

    // Move a linha do step k para `newCents`, presa entre as vizinhas (não cruza). O step 0 não
    // se move. Devolve a posição aplicada. Se a tabela não vale para currentEdo, começa da igual.
    double moveStep (int step, double newCents, int currentEdo) noexcept
    {
        if (! isFor (currentEdo))
            *this = equal (currentEdo);

        if (step <= 0 || step >= edo)
            return step == 0 ? 0.0 : newCents;

        const auto index = static_cast<std::size_t> (step);
        const double low  = cents[index - 1] + minGapCents;
        const double high = (step + 1 < edo ? cents[index + 1] : centsPerOctave) - minGapCents;

        cents[index] = std::clamp (newCents, low, std::max (low, high));
        return cents[index];
    }

    // --- Texto salvo no estado do plugin: "N:c1,c2,...,cN-1" (o step 0 é sempre 0). ---

    std::string toString() const
    {
        std::ostringstream out;
        out.precision (10);
        out << edo << ':';
        for (int k = 1; k < edo; ++k)
            out << (k > 1 ? "," : "") << cents[static_cast<std::size_t> (k)];
        return out.str();
    }

    // Texto inválido (vazio, N fora da faixa, número errado de valores, linhas fora de ordem)
    // devolve nada: o plugin fica na afinação igual em vez de tocar algo estranho.
    static std::optional<TuningTable> fromString (const std::string& text)
    {
        std::istringstream in (text);
        int n = 0;
        char colon = 0;
        if (! (in >> n >> colon) || colon != ':' || n < 1 || n > maxEdo)
            return std::nullopt;

        TuningTable table;
        table.edo = n;

        for (int k = 1; k < n; ++k)
        {
            if (k > 1)
            {
                char comma = 0;
                if (! (in >> comma) || comma != ',')
                    return std::nullopt;
            }

            double value = 0.0;
            if (! (in >> value) || ! std::isfinite (value))
                return std::nullopt;

            const double previous = table.cents[static_cast<std::size_t> (k - 1)];
            if (value < previous + minGapCents * 0.5 || value > centsPerOctave - minGapCents * 0.5)
                return std::nullopt;

            table.cents[static_cast<std::size_t> (k)] = value;
        }

        in >> std::ws;
        if (! in.eof())
            return std::nullopt;

        return table;
    }

private:
    // Cents do step inteiro i (qualquer sinal): oitava × 1200 + posição na tabela.
    double wholeStepCents (int i, int n) const noexcept
    {
        // floorDiv / floorMod: -1 vira step n-1 da oitava -1 (como Math.floorMod em Java).
        int octave = i / n;
        int step   = i % n;
        if (step < 0)
        {
            step += n;
            --octave;
        }
        return octave * centsPerOctave + cents[static_cast<std::size_t> (step)];
    }
};
