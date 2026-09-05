#pragma once

#include "../GifImportTypes.hpp"

#include <cstdint>
#include <vector>

namespace paimon::gifimport {

// Traza un color con elipses y nada mas: la mas grande que quepa donde la mancha
// es mas gorda, estirada hacia donde la mancha da de si, y asi hasta no dejar
// celda sin pintar. Todos los objetos salen del mismo circulo de GD, o sea de la
// misma hoja de sprites, asi que aqui el orden Z entre ellos si manda y ninguno
// tiene que quedarse debajo de un cuadrado.
std::vector<Primitive> vectorizeCircles(
    std::vector<int> const& positions,
    int width,
    int height,
    int color,
    int rank,
    std::vector<std::uint8_t> const& blocked = {},
    std::vector<std::uint8_t> const& empty = {}
);

} // namespace paimon::gifimport
