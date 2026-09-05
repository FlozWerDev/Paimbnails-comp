#include "VersusRanks.hpp"
#include "../../progression/data/ProgressionTiers.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace paimon::versus {

namespace {

// Lower bound of every tier. Paimon has none: the server grants it to the top
// twenty of each mode and takes it away when they drop out.
constexpr std::array<int, 20> kFloor = {
       0,  800,  900, 1000, 1100, 1200, 1300, 1400, 1500, 1600,
    1700, 1800, 1900, 2000, 2125, 2250, 2375, 2500, 2700, 2700,
};

// Divisions stop at Paragon; above it the ladder is a single list per tier.
constexpr int kLastDividedTier = 12;

int ceilingOf(int tier) {
    if (tier >= kPaimonTier - 1) return kFloor[kPaimonTier - 1] + 400;
    return kFloor[tier + 1];
}

} // namespace

float RankInfo::tierProgress() const {
    if (tierSpan <= 0) return 1.f;
    return std::clamp(static_cast<float>(intoTier) / static_cast<float>(tierSpan), 0.f, 1.f);
}

RankInfo rankFor(int elo, int placementsLeft, bool paimon) {
    RankInfo rank;
    rank.elo = std::max(0, elo);
    rank.placementsLeft = std::max(0, placementsLeft);

    if (paimon) {
        rank.tierIndex = kPaimonTier;
        rank.tierSpan = 1;
        rank.intoTier = 1;
        return rank;
    }

    int tier = 0;
    for (int i = static_cast<int>(kFloor.size()) - 2; i >= 0; i--) {
        if (rank.elo >= kFloor[i]) { tier = i; break; }
    }

    rank.tierIndex = tier;
    rank.tierSpan = std::max(1, ceilingOf(tier) - kFloor[tier]);
    rank.intoTier = rank.elo - kFloor[tier];

    if (tier <= kLastDividedTier) {
        int const step = std::max(1, rank.tierSpan / 4);
        int const idx = std::min(3, rank.intoTier / step);
        rank.division = 4 - idx;
    }
    return rank;
}

int kFactor(int elo, int placementsLeft) {
    if (placementsLeft > 0) return 48;
    if (elo < 1500) return 32;
    if (elo < 2250) return 16;
    return 12;
}

float expectedScore(int own, int rival) {
    return 1.f / (1.f + std::pow(10.f, static_cast<float>(rival - own) / 400.f));
}

int eloDelta(int own, int rival, bool won, int placementsLeft, int streak, float margin) {
    float const expected = expectedScore(own, rival);
    float change = static_cast<float>(kFactor(own, placementsLeft)) * ((won ? 1.f : 0.f) - expected);

    if (won) {
        float bonus = 1.f;
        if (streak >= 3) bonus += 0.15f;
        bonus += std::clamp(margin / 60.f, 0.f, 1.f) * 0.20f;
        change *= std::min(bonus, 1.35f);
    }

    int const rounded = static_cast<int>(std::lround(change));
    // A win always pays something, or a heavy favourite would grind for nothing.
    if (won && rounded < 1) return 1;
    if (!won && rounded > -1) return std::max(-1, -own);
    return std::max(rounded, -own);
}

int decayFor(int elo, int daysIdle) {
    RankInfo const rank = rankFor(elo);
    if (rank.tierIndex < kDecayFloorTier) return 0;

    int const period = rank.tierIndex >= 18 ? 7 : 10;
    if (daysIdle < period) return 0;
    return -25 * (daysIdle / period);
}

int softReset(int elo) {
    return static_cast<int>(std::lround(kStartElo + (elo - kStartElo) * 0.55));
}

std::string rankName(RankInfo const& rank) {
    auto const& tier = progression::tierAt(rank.tierIndex);
    if (!rank.hasDivision()) return tier.name;

    static char const* kNumerals[] = {"", "I", "II", "III", "IV"};
    return fmt::format("{} {}", tier.name, kNumerals[std::clamp(rank.division, 1, 4)]);
}

std::string rankShortName(RankInfo const& rank) {
    return progression::tierAt(rank.tierIndex).name;
}

cocos2d::ccColor3B rankColor(RankInfo const& rank) {
    return progression::tierAt(rank.tierIndex).base;
}

} // namespace paimon::versus
