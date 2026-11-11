#include "ShapePoints.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace
{
    // Quanto a curvatura máxima (|curve| = 1) entorta o segmento.
    constexpr double curveStrength = 8.0;

    // Curvatura que aproxima um quarto de senoide (sobe rápido e achata no topo).
    constexpr float sineCurve = -0.211f; // menor erro máximo (~0,043) contra sin(πt/2), calculado
}

ShapePoints ShapePoints::preset (Preset preset)
{
    ShapePoints shape;
    auto set = [&shape] (std::initializer_list<Point> list)
    {
        shape.count = 0;
        for (const auto& p : list)
            shape.points[static_cast<std::size_t> (shape.count++)] = p;
    };

    switch (preset)
    {
        case Preset::sine:
            // Cada quarto de ciclo é um segmento curvo: sobe rápido de 0 e achata no pico.
            set ({ { 0.0f, 0.0f, sineCurve }, { 0.25f, 1.0f, -sineCurve }, { 0.5f, 0.0f, sineCurve },
                   { 0.75f, -1.0f, -sineCurve }, { 1.0f, 0.0f, 0.0f } });
            break;
        case Preset::triangleUp:   set ({ { 0.0f, 0.0f }, { 0.25f, 1.0f }, { 0.75f, -1.0f }, { 1.0f, 0.0f } }); break;
        case Preset::triangleDown: set ({ { 0.0f, 0.0f }, { 0.25f, -1.0f }, { 0.75f, 1.0f }, { 1.0f, 0.0f } }); break;
        case Preset::sawUp:        set ({ { 0.0f, -1.0f }, { 1.0f, 1.0f } }); break;
        case Preset::sawDown:      set ({ { 0.0f, 1.0f }, { 1.0f, -1.0f } }); break;
        case Preset::square:
            // Dois pontos no mesmo x fazem um salto vertical.
            set ({ { 0.0f, 1.0f }, { 0.5f, 1.0f }, { 0.5f, -1.0f }, { 1.0f, -1.0f } });
            break;
        case Preset::flat:         set ({ { 0.0f, 0.0f }, { 1.0f, 0.0f } }); break;
    }

    return shape;
}

int ShapePoints::add (float x, float y)
{
    if (count >= maxPoints)
        return -1;

    x = std::clamp (x, 0.0f, 1.0f);
    y = std::clamp (y, -1.0f, 1.0f);

    // Posição de inserção: depois do último ponto com x <= novo x, mas nunca antes do primeiro
    // nem depois do último (que ficam presos em 0 e 1).
    int index = 1;
    while (index < count - 1 && points[static_cast<std::size_t> (index)].x <= x)
        ++index;

    for (int i = count; i > index; --i)
        points[static_cast<std::size_t> (i)] = points[static_cast<std::size_t> (i - 1)];

    points[static_cast<std::size_t> (index)] = { x, y, 0.0f };
    ++count;
    return index;
}

void ShapePoints::move (int index, float x, float y)
{
    if (index < 0 || index >= count)
        return;

    auto& p = points[static_cast<std::size_t> (index)];
    p.y = std::clamp (y, -1.0f, 1.0f);

    if (index == 0 || index == count - 1)
        return; // pontas presas em x = 0 e x = 1

    const float left  = points[static_cast<std::size_t> (index - 1)].x;
    const float right = points[static_cast<std::size_t> (index + 1)].x;
    p.x = std::clamp (x, left, right);
}

bool ShapePoints::remove (int index)
{
    if (index <= 0 || index >= count - 1)
        return false;

    for (int i = index; i < count - 1; ++i)
        points[static_cast<std::size_t> (i)] = points[static_cast<std::size_t> (i + 1)];

    --count;
    return true;
}

void ShapePoints::setCurve (int index, float curve)
{
    if (index >= 0 && index < count - 1)
        points[static_cast<std::size_t> (index)].curve = std::clamp (curve, -1.0f, 1.0f);
}

double ShapePoints::curveT (double t, float curve) noexcept
{
    if (std::abs (curve) < 1.0e-4f)
        return t;

    // Exponencial normalizada: passa por (0, 0) e (1, 1) e entorta conforme k.
    const double k = curve * curveStrength;
    return (std::exp (k * t) - 1.0) / (std::exp (k) - 1.0);
}

float ShapePoints::evaluate (double x) const noexcept
{
    if (count == 0)
        return 0.0f;

    x = std::clamp (x, 0.0, 1.0);

    // Último ponto com x <= posição: o segmento começa nele. Em saltos verticais (dois pontos
    // no mesmo x), vale o de baixo na lista, ou seja, o valor depois do salto.
    int i = 0;
    while (i < count - 2 && points[static_cast<std::size_t> (i + 1)].x <= x)
        ++i;

    const auto& a = points[static_cast<std::size_t> (i)];
    const auto& b = points[static_cast<std::size_t> (i + 1)];
    const double width = b.x - a.x;
    const double t = width > 0.0 ? std::clamp ((x - a.x) / width, 0.0, 1.0) : 1.0;

    return static_cast<float> (a.y + (b.y - a.y) * curveT (t, a.curve));
}

std::string ShapePoints::toString() const
{
    std::string text;
    char buffer[64];

    for (int i = 0; i < count; ++i)
    {
        const auto& p = points[static_cast<std::size_t> (i)];
        std::snprintf (buffer, sizeof (buffer), "%s%.5g,%.5g,%.5g", i == 0 ? "" : ";", p.x, p.y, p.curve);
        text += buffer;
    }

    return text;
}

std::optional<ShapePoints> ShapePoints::fromString (const std::string& text)
{
    ShapePoints shape;
    shape.count = 0;

    std::stringstream stream (text);
    std::string item;

    while (std::getline (stream, item, ';'))
    {
        Point p;
        char comma1 = 0, comma2 = 0;
        std::stringstream itemStream (item);

        if (! (itemStream >> p.x >> comma1 >> p.y >> comma2 >> p.curve) || comma1 != ',' || comma2 != ',')
            return std::nullopt;

        if (shape.count >= maxPoints)
            return std::nullopt;

        shape.points[static_cast<std::size_t> (shape.count++)] = p;
    }

    if (shape.count < 2)
        return std::nullopt;

    // Garante as regras mesmo com texto editado à mão: pontas em 0 e 1, x em ordem, faixas.
    for (int i = 0; i < shape.count; ++i)
    {
        auto& p = shape.points[static_cast<std::size_t> (i)];
        p.y     = std::clamp (p.y, -1.0f, 1.0f);
        p.curve = std::clamp (p.curve, -1.0f, 1.0f);
        p.x     = std::clamp (p.x, i == 0 ? 0.0f : shape.points[static_cast<std::size_t> (i - 1)].x, 1.0f);
    }
    shape.points[0].x = 0.0f;
    shape.points[static_cast<std::size_t> (shape.count - 1)].x = 1.0f;

    return shape;
}

bool ShapePoints::operator== (const ShapePoints& other) const noexcept
{
    if (count != other.count)
        return false;

    for (int i = 0; i < count; ++i)
    {
        const auto& a = points[static_cast<std::size_t> (i)];
        const auto& b = other.points[static_cast<std::size_t> (i)];
        if (a.x != b.x || a.y != b.y || a.curve != b.curve)
            return false;
    }

    return true;
}
