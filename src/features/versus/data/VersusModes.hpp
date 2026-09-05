#pragma once

#include "VersusTypes.hpp"

#include <array>
#include <string>
#include <vector>

namespace paimon::versus {

struct FormatDef {
    Format id;
    char const* key;
    char const* name;
    uint8_t modes;
    int timeLimit;      // seconds, 0 when the format has no clock
    int attemptLimit;   // 0 when attempts are unlimited
    bool ranked;        // Friendly is the only one that never moves Elo
    bool cards;         // only Roulette deals
};

inline constexpr size_t kFormatCount = 10;

std::array<FormatDef, kFormatCount> const& allFormats();
FormatDef const& formatAt(Format format);
FormatDef const* findFormat(std::string const& key);

// Formats that can be queued for in this mode, Friendly excluded.
std::vector<FormatDef const*> rankedFormats(Mode mode);

std::string formatName(FormatDef const& def);
std::string formatWinCondition(FormatDef const& def);
std::string formatSprite(FormatDef const& def);

// Ladder cuts the level at 25/50/75/100; the bit for a segment is set once
// somebody crosses it, and it is never handed to the second one there.
inline constexpr int kLadderSegments = 4;
inline constexpr int kLadderToWin = 3;
int segmentForPercent(float percent);

// King of the hill: how long the lead has to hold.
inline constexpr float kHillSeconds = 45.f;

} // namespace paimon::versus
