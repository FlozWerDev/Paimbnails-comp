#pragma once

#include <Geode/cocos/cocoa/CCGeometry.h>
#include <Geode/cocos/include/ccTypes.h>
#include <array>
#include <cstdint>

namespace paimon::progression {

inline constexpr int kMaxLevel = 200;
inline constexpr int kLevelsPerTier = 10;
inline constexpr int kTierCount = kMaxLevel / kLevelsPerTier;

// Frame silhouette drawn behind the tier glyph. Changes every four tiers so the
// badge silhouette alone reads as progress from across the screen.
enum class TierFrame {
    Pill,
    Shield,
    Hexagon,
    Star,
    Crown,
};

enum TierEffect : uint32_t {
    TierEffectNone   = 0,
    TierEffectGlow   = 1u << 0,
    TierEffectSweep  = 1u << 1,
    TierEffectPulse  = 1u << 2,
    TierEffectSparks = 1u << 3,
    TierEffectOrbit  = 1u << 4,
};

struct Tier {
    char const* id;
    char const* name;
    int index;
    cocos2d::ccColor3B base;
    cocos2d::ccColor3B accent;
    TierFrame frame;
    uint32_t effects;
};

// Curve: exp(L) = 50(L-1)^2 + 150(L-1). Level 1 is free, level 2 costs 200 and
// a maxed-out account lands around 150, so 200 stays out of reach on purpose.
int64_t expForLevel(int level);
int levelForExp(int64_t exp);

// Span and offset of `exp` inside its own level, for the XP bar.
int64_t expSpanOfLevel(int level);
int64_t expIntoLevel(int64_t exp);
float levelProgress(int64_t exp);

std::array<Tier, kTierCount> const& allTiers();
Tier const& tierAt(int index);
Tier const& tierForLevel(int level);
int tierIndexForLevel(int level);

// First level of the tier after `index`, or kMaxLevel when there is none.
int nextTierLevel(int index);

} // namespace paimon::progression
