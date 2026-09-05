#pragma once

#include "../data/VersusCards.hpp"

#include <cstdint>
#include <vector>

namespace paimon::versus {

// PCG32. The server hands out one seed per match before the countdown, so both
// clients build the same milestone list without a round trip and a card lands
// the instant the percentage is crossed.
class VersusRng {
public:
    explicit VersusRng(uint64_t seed);

    uint32_t next();
    uint32_t below(uint32_t bound);
    float unit();

private:
    uint64_t m_state = 0;
    uint64_t m_inc = 0;
};

inline constexpr int kMinMilestones = 6;
inline constexpr int kMaxMilestones = 9;
inline constexpr float kMilestoneGap = 6.f;

std::vector<float> rollMilestones(uint64_t seed);

// The card a milestone deals. Each side rolls its own and announces it, so the
// deficit shift can use local numbers without the two clients having to agree.
CardId rollCard(uint64_t seed, int milestone, uint8_t modeMask, float deficit, bool catchUp);

} // namespace paimon::versus
