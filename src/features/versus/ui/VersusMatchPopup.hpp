#pragma once

#include "../data/VersusTypes.hpp"
#include "VersusRankBadgeNode.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>

#include <vector>

namespace paimon::versus {

// One modal for the whole run-up to a duel: the rival appears, both accept,
// each vetoes a level, and the level opens. Splitting it in three popups would
// only make the screen flicker between steps that are seconds apart.
class VersusMatchPopup : public geode::Popup {
public:
    static VersusMatchPopup* create();

protected:
    bool init() override;
    void onEnter() override;
    void onExit() override;

    void rebuild();
    void buildFound(cocos2d::CCNode* page);
    void buildBanning(cocos2d::CCNode* page);
    void buildLoading(cocos2d::CCNode* page);

    void onAccept(cocos2d::CCObject* sender);
    void onDecline(cocos2d::CCObject* sender);
    void onBan(cocos2d::CCObject* sender);
    void onPlay(cocos2d::CCObject* sender);

    Phase m_drawn = Phase::Idle;
    cocos2d::CCNode* m_page = nullptr;
    cocos2d::CCMenu* m_menu = nullptr;
};

} // namespace paimon::versus
