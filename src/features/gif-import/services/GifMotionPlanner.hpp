#pragma once

#include "../GifImportTypes.hpp"

namespace paimon::gifimport {

// Una silueta seguida por varios frames, con el color de cada celda de la pose
// de referencia y donde cae esa pose en cada frame.
struct MotionGroup {
    std::vector<std::uint64_t> mask;
    std::vector<MotionKey> keys;
    std::vector<int> positions;
    std::vector<std::int32_t> colors;
};

struct MotionAnalysis {
    std::vector<MotionGroup> groups;
    std::vector<GridFrame> residual;
};

inline bool visibleAt(std::vector<std::uint64_t> const& mask, int frame) {
    if (mask.empty()) return false;
    return (mask[static_cast<std::size_t>(frame / 64)] &
            (std::uint64_t{1} << (frame % 64))) != 0;
}

inline MotionKey const* keyAt(MotionTrack const& track, int frame) {
    for (auto const& key : track.keys) {
        if (key.frame == frame) return &key;
    }
    return nullptr;
}

// Busca siluetas que el frame siguiente repite identicas pero corridas de sitio.
// Lo que encuentra sale en `groups` y desaparece de `residual`, donde el hueco
// que dejan se rellena con el fondo que tapaban: asi el fondo vuelve a ser el
// mismo en todos los frames y deja de costar un objeto por frame.
MotionAnalysis analyzeMotion(
    std::vector<GridFrame> const& frames,
    int width,
    int height
);

std::size_t motionMoveCount(
    std::vector<MotionTrack> const& tracks,
    int frames,
    bool loop
);

std::size_t motionTriggerCount(
    std::vector<MotionTrack> const& tracks,
    int frames,
    bool loop
);

} // namespace paimon::gifimport
