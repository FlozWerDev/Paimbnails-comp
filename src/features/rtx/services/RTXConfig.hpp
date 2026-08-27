#pragma once

// Configuracion de Paimon RTX. Vive en un JSON propio (rtx_config.json) porque
// son demasiados campos para un saved value por cada uno.

#include <string>

namespace paimon::rtx {

enum class Preset : int {
    Performance = 0,
    Balanced    = 1,
    Quality     = 2,
    Ultra       = 3,
    Custom      = 4,
};

enum class Tonemap : int {
    None       = 0,
    Reinhard   = 1,
    ACES       = 2,
    Filmic     = 3,
    Uncharted2 = 4,
};

struct RTXConfig {
    bool  enabled          = false;
    float intensity        = 0.85f;

    int   preset           = static_cast<int>(Preset::Balanced);
    float renderScale      = 0.50f;
    int   rayCount         = 4;
    int   raySteps         = 12;
    float rayDistance      = 0.28f;
    float stepGrowth       = 1.25f;
    bool  adaptive         = true;
    int   targetFps        = 60;
    int   frameSkip        = 0;

    float giStrength       = 0.65f;
    float giSaturation     = 1.00f;
    float lightThreshold   = 0.60f;
    float lightRange       = 0.35f;
    float bounceFalloff    = 2.20f;
    float normalStrength   = 6.00f;
    float thickness        = 0.35f;

    float aoStrength       = 0.45f;
    float aoRadius         = 0.12f;
    float aoPower          = 1.40f;

    float reflectStrength  = 0.30f;
    float reflectRoughness = 0.25f;
    float reflectFresnel   = 0.70f;
    float reflectFade      = 0.45f;

    float bloomStrength    = 0.45f;
    float bloomThreshold   = 0.62f;
    float bloomRadius      = 2.00f;
    int   bloomPasses      = 4;

    float godRayStrength   = 0.25f;
    float godRayDecay      = 0.94f;
    float godRayDensity    = 0.85f;
    float godRayX          = 0.50f;
    float godRayY          = 0.82f;

    float denoise          = 1.60f;
    int   atrousPasses     = 3;
    float temporal         = 0.88f;
    bool  ghostClamp       = true;
    float clampSigma       = 1.40f;

    int   tonemap          = static_cast<int>(Tonemap::ACES);
    float exposure         = 0.05f;
    float contrast         = 1.06f;
    float saturation       = 1.10f;
    float temperature      = 0.06f;
    float tint             = 0.00f;
    float gamma            = 1.00f;

    float chromatic        = 0.15f;
    float vignette         = 0.35f;
    float grain            = 0.10f;
    float sharpen          = 0.20f;

    bool  inGameplay       = true;
    bool  inEditor         = false;
    bool  inMenus          = true;
    bool  skipWhenPaused   = false;
};

// Sobrescribe solo los campos de coste (resolucion, rayos, pases); el resto del
// look que el usuario haya tocado se conserva. Custom no toca nada.
void applyPreset(RTXConfig& cfg, Preset preset);

char const* presetName(int preset);
char const* tonemapName(int tonemap);

} // namespace paimon::rtx
