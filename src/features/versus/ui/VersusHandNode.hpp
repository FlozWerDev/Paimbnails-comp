#pragma once

#include "../data/VersusCards.hpp"

#include <Geode/Geode.hpp>

#include <vector>

namespace paimon::versus {

// The two card slots and the row of active effects, bottom right of the level.
// It reads the session every frame rather than being told, so a card dealt by
// crossing a milestone shows up without anything having to call in.
class VersusHandNode : public cocos2d::CCNode {
public:
    static VersusHandNode* create();

    void refresh();

protected:
    bool init() override;
    void update(float dt) override;
    void rebuildHand();
    void rebuildEffects();

    std::vector<CardId> m_drawn;
    cocos2d::CCNode* m_slots = nullptr;
    cocos2d::CCNode* m_effects = nullptr;
    cocos2d::CCLabelBMFont* m_locked = nullptr;
    size_t m_effectCount = 0;
};

} // namespace paimon::versus
