#pragma once
// Renderiza un slot ("zona de color") de un proyecto: coloca cada pieza en el
// lienzo, le aplica su relleno y las compone en orden.

#include "../data/IconProject.hpp"
#include "../../texture-studio/data/ImageBuffer.hpp"

#include <Geode/Geode.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace paimon::icon_maker {

// One piece as the editor needs it: its own pixels for the layer thumbnail, a
// small alpha mask so a touch on the canvas can tell which layer it hit, and
// the alpha box the selection frame is drawn around.
struct PieceRender {
    int index = -1;
    std::string pieceId;
    bool visible = true;
    texture_studio::ImageBuffer pixels;

    std::vector<std::uint8_t> mask;
    int maskSize = 0;

    // Lado del lienzo en el que se midio la caja; el editor libera `pixels`
    // en cuanto tiene la miniatura, asi que no se puede deducir de ahi.
    int canvasSize = 0;

    // Canvas pixels, rows running top-down like the buffer.
    int boundsX = 0;
    int boundsY = 0;
    int boundsW = 0;
    int boundsH = 0;
    bool hasBounds = false;
};

struct SlotRender {
    texture_studio::ImageBuffer composite;
    std::vector<PieceRender> pieces;
};

class PieceRenderer final {
public:
    // Canvas is square (anatomy canvasUhd). Missing piece files are skipped
    // with a log instead of failing the whole slot.
    static geode::Result<texture_studio::ImageBuffer> renderSlot(
        IconProject const& project,
        std::string const& slotKey,
        int canvasSize,
        std::filesystem::path const& imagesDir);

    // Same composite as renderSlot, plus the per-piece data the editor's canvas
    // and layer list need. Hidden pieces are rendered (the list still shows
    // them) but neither composited nor given a mask.
    static SlotRender renderSlotDetailed(
        IconProject const& project,
        std::string const& slotKey,
        int canvasSize,
        int maskSize,
        std::filesystem::path const& imagesDir);

    // One placed + filled piece on a canvasSize canvas.
    static geode::Result<texture_studio::ImageBuffer> renderPiece(
        IconPiece const& piece,
        int canvasSize,
        int guideSize,
        std::filesystem::path const& imagesDir);

private:
    PieceRenderer() = delete;
};

}  // namespace paimon::icon_maker
