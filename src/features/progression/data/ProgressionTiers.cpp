#include "ProgressionTiers.hpp"

#include <algorithm>
#include <cmath>

namespace paimon::progression {

namespace {

using F = TierFrame;

constexpr uint32_t kGlow   = TierEffectGlow;
constexpr uint32_t kSweep  = TierEffectGlow | TierEffectSweep;
constexpr uint32_t kPulse  = kSweep | TierEffectPulse;
constexpr uint32_t kSparks = kPulse | TierEffectSparks;
constexpr uint32_t kOrbit  = kSparks | TierEffectOrbit;

constexpr std::array<Tier, kTierCount> kTiers = {{
    {"wanderer",    "Wanderer",     0, {150, 160, 175}, {205, 214, 228}, F::Pill,    TierEffectNone},
    {"novice",      "Novice",       1, { 92, 190, 120}, {172, 240, 192}, F::Pill,    TierEffectNone},
    {"apprentice",  "Apprentice",   2, { 72, 190, 190}, {160, 240, 240}, F::Pill,    TierEffectNone},
    {"adept",       "Adept",        3, { 80, 150, 240}, {172, 206, 255}, F::Shield,  TierEffectNone},
    {"skilled",     "Skilled",      4, {110, 120, 240}, {182, 190, 255}, F::Shield,  TierEffectNone},
    {"veteran",     "Veteran",      5, {160, 105, 235}, {216, 176, 255}, F::Shield,  kGlow},
    {"elite",       "Elite",        6, {215,  90, 200}, {250, 176, 240}, F::Shield,  kGlow},
    {"champion",    "Champion",     7, {230,  75, 100}, {255, 166, 176}, F::Hexagon, kGlow},
    {"master",      "Master",       8, {245, 140,  55}, {255, 206, 150}, F::Hexagon, kSweep},
    {"grandmaster", "Grandmaster",  9, {250, 200,  60}, {255, 241, 170}, F::Hexagon, kSweep},
    {"ascendant",   "Ascendant",   10, {170, 225,  70}, {226, 250, 170}, F::Star,    kSweep},
    {"luminary",    "Luminary",    11, { 90, 225, 215}, {186, 255, 250}, F::Star,    kSweep},
    {"paragon",     "Paragon",     12, { 60, 170, 255}, {172, 220, 255}, F::Star,    kPulse},
    {"mythic",      "Mythic",      13, {140,  90, 255}, {206, 176, 255}, F::Star,    kPulse},
    {"celestial",   "Celestial",   14, {255, 120, 190}, {255, 196, 226}, F::Crown,   kPulse},
    {"eclipse",     "Eclipse",     15, {200,  40,  60}, {255, 140, 120}, F::Crown,   kSparks},
    {"nebula",      "Nebula",      16, {120,  60, 220}, {230, 140, 255}, F::Crown,   kSparks},
    {"singularity", "Singularity", 17, { 46,  50,  78}, {120, 255, 240}, F::Crown,   kOrbit},
    {"eternal",     "Eternal",     18, {255, 235, 190}, {255, 255, 255}, F::Crown,   kOrbit},
    {"paimon",      "Paimon",      19, { 90, 160, 255}, {255, 226, 140}, F::Crown,   kOrbit},
}};

} // namespace

int64_t expForLevel(int level) {
    if (level <= 1) return 0;
    int64_t const n = std::min(level, kMaxLevel) - 1;
    return 50 * n * n + 150 * n;
}

int levelForExp(int64_t exp) {
    if (exp <= 0) return 1;
    double const n = (std::sqrt(22500.0 + 200.0 * static_cast<double>(exp)) - 150.0) / 100.0;
    return std::clamp(static_cast<int>(n) + 1, 1, kMaxLevel);
}

int64_t expSpanOfLevel(int level) {
    if (level >= kMaxLevel) return 0;
    return expForLevel(level + 1) - expForLevel(level);
}

int64_t expIntoLevel(int64_t exp) {
    return std::max<int64_t>(0, exp - expForLevel(levelForExp(exp)));
}

float levelProgress(int64_t exp) {
    int const level = levelForExp(exp);
    int64_t const span = expSpanOfLevel(level);
    if (span <= 0) return 1.f;
    return std::clamp(static_cast<float>(expIntoLevel(exp)) / static_cast<float>(span), 0.f, 1.f);
}

std::array<Tier, kTierCount> const& allTiers() {
    return kTiers;
}

Tier const& tierAt(int index) {
    return kTiers[std::clamp(index, 0, kTierCount - 1)];
}

int tierIndexForLevel(int level) {
    return std::clamp((std::clamp(level, 1, kMaxLevel) - 1) / kLevelsPerTier, 0, kTierCount - 1);
}

Tier const& tierForLevel(int level) {
    return tierAt(tierIndexForLevel(level));
}

int nextTierLevel(int index) {
    if (index >= kTierCount - 1) return kMaxLevel;
    return (index + 1) * kLevelsPerTier + 1;
}

} // namespace paimon::progression
