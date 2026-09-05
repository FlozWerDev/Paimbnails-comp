#pragma once

#include "../data/VersusRanks.hpp"

#include <Geode/Geode.hpp>

namespace paimon::versus {

// The VS badge: the laurel-and-swords frame behind one of the progression tier
// plates, with a row of division pips under it. Unranked players get the frame
// greyed out and a placement counter instead of pips.
class VersusRankBadgeNode : public cocos2d::CCNode {
public:
    static VersusRankBadgeNode* create(RankInfo const& rank, float size);

    void setRank(RankInfo const& rank);
    void setShowPips(bool show);
    // Greys the badge out for a player who has never duelled.
    void setDim(bool dim);
    void playPromotion();

    RankInfo const& rank() const { return m_rank; }

protected:
    bool init(RankInfo const& rank, float size);
    void rebuild();

    RankInfo m_rank;
    float m_size = 48.f;
    bool m_showPips = true;
    bool m_dim = false;
    cocos2d::CCNode* m_content = nullptr;
};

} // namespace paimon::versus
