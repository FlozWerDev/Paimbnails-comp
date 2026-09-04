#pragma once

#include "../data/ProgressionBadges.hpp"

#include <optional>
#include <string>
#include <vector>

class GJUserScore;

namespace paimon::progression {

struct ProgressDelta {
    int64_t gainedExp = 0;
    int64_t totalExp = 0;
    int fromLevel = 1;
    int toLevel = 1;
    std::vector<BadgeDef const*> newBadges;

    bool leveledUp() const { return toLevel > fromLevel; }
    bool tierChanged() const;
};

class ProgressionService {
public:
    static ProgressionService& get();

    bool enabled() const;

    // GameStatsManager for the live six, the last synced score for everything
    // the save file doesn't track (demon breakdown, creator points, rank).
    BadgeContext ownContext();

    // Keeps the parts of a score the local save can't rebuild.
    void rememberOwnScore(GJUserScore* score);

    // Compares the live numbers against the stored snapshot and commits the new
    // one. Returns nothing on the first ever call (nothing to compare against)
    // or when nothing moved.
    std::optional<ProgressDelta> consumeDelta();

    void commitSnapshot();
    bool hasSnapshot() const;

private:
    ProgressionService() = default;

    std::vector<std::string> storedBadges() const;
    void storeBadges(BadgeContext const& ctx);
};

} // namespace paimon::progression
