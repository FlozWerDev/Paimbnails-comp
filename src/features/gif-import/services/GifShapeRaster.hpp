#pragma once

#include "../GifImportTypes.hpp"
#include "GifVectorMath.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace paimon::gifimport {

// El giro de un objeto resuelto una sola vez. Preguntar si un punto cae dentro
// de una figura es de largo lo que mas se repite del importador —cientos de
// millones de veces en una imagen grande— y sacar el seno y el coseno en cada
// pregunta se llevaba, medido, tres cuartas partes del tiempo de la importacion.
struct ShapeXform {
    float x = 0.f;
    float y = 0.f;
    float width = 0.f;
    float height = 0.f;
    float cosine = 1.f;
    float sine = 0.f;
    float extentX = 0.f;
    float extentY = 0.f;
    PrimitiveKind kind = PrimitiveKind::Block;
    StampMask const* mask = nullptr;

    bool contains(float px, float py) const {
        if (width <= 0.f || height <= 0.f) return false;
        float const dx = px - x;
        float const dy = py - y;
        float const localX = dx * cosine + dy * sine;
        float const localY = -dx * sine + dy * cosine;

        if (kind == PrimitiveKind::Circle) {
            float const nx = localX / (width * 0.5f);
            float const ny = localY / (height * 0.5f);
            return nx * nx + ny * ny <= 1.f;
        }
        if (kind == PrimitiveKind::Triangle || kind == PrimitiveKind::WideTriangle) {
            float const u = localX / width + 0.5f;
            float const v = localY / height + 0.5f;
            return u >= 0.f && v >= 0.f && u <= 1.f && v <= 1.f && u + v <= 1.f;
        }
        if (kind == PrimitiveKind::Stamp) {
            if (!mask || mask->empty()) return false;
            float const u = localX / width + 0.5f;
            float const v = localY / height + 0.5f;
            if (u < 0.f || v < 0.f || u >= 1.f || v >= 1.f) return false;
            int const cellX = std::min(
                mask->width - 1, static_cast<int>(u * mask->width));
            int const cellY = std::min(
                mask->height - 1, static_cast<int>(v * mask->height));
            return mask->coverage[
                static_cast<std::size_t>(cellY) * mask->width + cellX] >= 128;
        }
        return std::abs(localX) <= width * 0.5f && std::abs(localY) <= height * 0.5f;
    }
};

ShapeXform xformOf(Primitive const& object);
ShapeXform xformOf(Primitive const& object, std::vector<PlanStamp> const& stamps);
std::vector<ShapeXform> xformsOf(std::vector<Primitive> const& objects);

// Celdas que toca la caja envolvente, ya recortadas a la rejilla. Devuelve la
// caja vacia (max < min) cuando la figura se queda fuera.
std::array<int, 4> xformBox(ShapeXform const& shape, int width, int height);

// Una figura puede asomar de sus celdas solo hacia donde no se nota: celdas del
// mismo color, celdas que otro color tapa despues, o hueco que ningun frame
// pinta. Asomando sobre el color que queda debajo es cuando se ve el pico, y
// medir por el centro de la celda no lo detecta porque el pico entra menos de
// media celda.
bool shapeStaysInside(
    Primitive const& shape,
    std::vector<std::uint8_t> const& permitted,
    int width,
    int height
);

// Que parte de la figura cae fuera de lo permitido. Para una tira, exigir cero es
// pasarse: el bisel y el remate de una tira buena se salen un pico y no se ve,
// pero tirarla manda la mancha al contorno, que se pasa mucho mas.
float shapeSpill(
    Primitive const& shape,
    std::vector<std::uint8_t> const& permitted,
    int width,
    int height
);

// Tramo de x que la figura puede tocar en una fila. Sobra un pelo por los dos
// lados a proposito, asi que fuera de el se puede dar el punto por descartado
// sin preguntar.
bool xformSpan(ShapeXform const& shape, float y, float& fromX, float& toX);

// Recorre las muestras que la figura cubre en una rejilla de `scale` muestras
// por celda, en coordenadas de muestra. Devolver true desde `fn` corta.
template <typename Fn>
bool forEachSample(ShapeXform const& shape, int width, int height, int scale, Fn&& fn) {
    auto const box = xformBox(shape, width, height);
    if (box[2] < box[0] || box[3] < box[1]) return false;
    int const lowX = box[0] * scale;
    int const highX = (box[2] + 1) * scale - 1;
    float const step = 1.f / static_cast<float>(scale);
    for (int y = box[1] * scale; y <= (box[3] + 1) * scale - 1; ++y) {
        float const sampleY = (static_cast<float>(y) + 0.5f) * step;
        float spanFrom = 0.f;
        float spanTo = 0.f;
        if (!xformSpan(shape, sampleY, spanFrom, spanTo)) continue;
        int const first = std::max(
            lowX, static_cast<int>(std::floor(spanFrom * scale - 0.5f)));
        int const last = std::min(
            highX, static_cast<int>(std::ceil(spanTo * scale - 0.5f)));
        for (int x = first; x <= last; ++x) {
            if (!shape.contains((static_cast<float>(x) + 0.5f) * step, sampleY)) continue;
            if (fn(x, y)) return true;
        }
    }
    return false;
}

} // namespace paimon::gifimport
