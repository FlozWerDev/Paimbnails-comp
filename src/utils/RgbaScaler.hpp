#pragma once

#include <cstdint>
#include <memory>

namespace paimon::rgba {

// Scales tightly packed RGBA8888 pixels. The returned buffer is owned by the
// caller and uses the same channel order as the input. libyuv is used when it
// is available; the fallback keeps this utility usable in host-side tests.
std::unique_ptr<uint8_t[]> scale(
    uint8_t const* src,
    int srcWidth,
    int srcHeight,
    int dstWidth,
    int dstHeight
);

} // namespace paimon::rgba
