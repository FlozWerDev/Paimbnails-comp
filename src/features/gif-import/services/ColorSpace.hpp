#pragma once

#include "../GifImportTypes.hpp"

namespace paimon::gifimport {

// Distancia perceptual entre dos colores de la paleta por debajo de la cual
// dos entradas se consideran el mismo color a efectos del dibujo: de lejos no
// se distinguen y solo consiguen partir el dibujo en mas objetos. En OkLab el
// negro y el blanco estan a 1, y un paso apenas visible ronda 0.02.
constexpr float kPaletteMinDistance = 0.025f;

struct OkLab {
    float L = 0.f;
    float a = 0.f;
    float b = 0.f;
};

OkLab rgbToOkLab(Color color);
Color oklabToRgb(OkLab color);
float oklabDistance(OkLab const& first, OkLab const& second);

} // namespace paimon::gifimport
