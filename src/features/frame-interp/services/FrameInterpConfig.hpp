#pragma once

// Configuracion de la interpolacion de fotogramas. Vive en su propio JSON
// (frame_interp.json) igual que la de RTX.

namespace paimon::frameinterp {

// Cuanto se deja atras lo dibujado respecto a lo simulado, medido en pasos de
// fisica. Con un paso entero nunca hace falta extrapolar; con cero se dibuja el
// presente exacto a costa de adivinar el ultimo tramo.
enum class Latency : int {
    Smooth   = 0,
    Balanced = 1,
    Instant  = 2,
};

struct FrameInterpConfig {
    bool  enabled       = false;
    bool  camera        = true;
    bool  scenery       = true;
    bool  players       = true;
    bool  movingObjects = false;

    int   latency       = static_cast<int>(Latency::Smooth);
    float strength      = 1.00f;
    int   objectLimit   = 400;

    bool  inGameplay    = true;
    bool  inEditor      = true;
};

// Fraccion de paso que se deja de retraso para cada modo.
double latencyLag(int latency);

} // namespace paimon::frameinterp
