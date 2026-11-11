#pragma once

#include <array>
#include <optional>
#include <string>

// Forma desenhada à mão por pontos (RF15), usada pelos LFOs e pelos osciladores.
// C++ puro, sem JUCE. Tamanho fixo (array, sem alocação): pode ser copiada à vontade.
//
// A forma é um ciclo: x vai de 0 (começo) a 1 (fim) e y de -1 a +1. Os pontos são ligados em
// ordem por segmentos; cada segmento pode ser reto ou curvo (curve, guardado no ponto onde ele
// começa). O primeiro ponto fica sempre em x = 0 e o último em x = 1; só a altura deles muda.
class ShapePoints
{
public:
    static constexpr int maxPoints = 32;

    struct Point
    {
        float x = 0.0f;     // 0..1
        float y = 0.0f;     // -1..+1
        float curve = 0.0f; // -1..+1, curvatura do segmento que começa aqui (0 = reta)
    };

    // Formas prontas que viram pontos editáveis. As dos osciladores e as dos LFOs têm o mesmo
    // desenho, exceto o triângulo: o do oscilador começa descendo, o do LFO começa subindo.
    enum class Preset { sine, triangleUp, triangleDown, sawUp, sawDown, square, flat };
    static ShapePoints preset (Preset preset);

    // Forma inicial: uma linha reta em 0 (dois pontos, nas pontas).
    ShapePoints()
    {
        points[0] = { 0.0f, 0.0f, 0.0f };
        points[1] = { 1.0f, 0.0f, 0.0f };
        count = 2;
    }

    int size() const noexcept { return count; }
    const Point& operator[] (int index) const noexcept { return points[static_cast<std::size_t> (index)]; }

    // Edição (thread da interface). x e y são limitados às faixas; x também fica entre os
    // vizinhos, para a ordem nunca se inverter.
    int  add (float x, float y);             // índice do ponto novo, ou -1 se já houver 32
    void move (int index, float x, float y); // o primeiro e o último só mudam de altura
    bool remove (int index);                 // o primeiro e o último não podem ser apagados
    void setCurve (int index, float curve);

    // Valor da forma na posição x (0..1). Valores fora da faixa são limitados.
    float evaluate (double x) const noexcept;

    // Curva de um segmento: t (0..1) entra reto e sai curvado. curve > 0 começa devagar e
    // acelera; curve < 0 começa rápido e freia. Público para os testes.
    static double curveT (double t, float curve) noexcept;

    // Texto para salvar no estado do plugin: "x,y,c;x,y,c;...". fromString devolve vazio
    // (std::optional ~ Optional do Java) se o texto for inválido.
    std::string toString() const;
    static std::optional<ShapePoints> fromString (const std::string& text);

    bool operator== (const ShapePoints& other) const noexcept;

private:
    std::array<Point, maxPoints> points {};
    int count = 0;
};
