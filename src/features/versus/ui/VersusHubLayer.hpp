#pragma once

#include "../data/VersusTypes.hpp"
#include "VersusRankBadgeNode.hpp"

#include <Geode/Geode.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>

#include <vector>

namespace paimon::versus {

class VersusHubLayer : public cocos2d::CCLayer {
public:
    static VersusHubLayer* create();
    static cocos2d::CCScene* scene();

protected:
    bool init() override;
    void onEnterTransitionDidFinish() override;
    void keyBackClicked() override;

    void buildChrome();
    void buildRankPanel();
    void buildFormatGrid();
    void buildActions();

    void refreshRank();
    void refreshFormats();
    void setStatus(std::string const& text, bool error = false);

    void onBack(cocos2d::CCObject* sender);
    void onMode(cocos2d::CCObject* sender);
    void onFormat(cocos2d::CCObject* sender);
    void onPlay(cocos2d::CCObject* sender);
    void onChallenge(cocos2d::CCObject* sender);
    void onDeck(cocos2d::CCObject* sender);
    void onLeaderboard(cocos2d::CCObject* sender);
    void onHistory(cocos2d::CCObject* sender);
    void onSeason(cocos2d::CCObject* sender);

    void authenticateThen(std::function<void()> next);
    void onSessionChanged();

    Mode m_mode = Mode::Classic;
    bool m_busy = false;
    bool m_matchPopupOpen = false;

    cocos2d::CCMenu* m_menu = nullptr;
    cocos2d::CCNode* m_rankPanel = nullptr;
    cocos2d::CCNode* m_formatPanel = nullptr;
    VersusRankBadgeNode* m_badge = nullptr;
    cocos2d::CCLabelBMFont* m_rankLabel = nullptr;
    cocos2d::CCLabelBMFont* m_eloLabel = nullptr;
    cocos2d::CCLabelBMFont* m_recordLabel = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;
    std::vector<CCMenuItemSpriteExtra*> m_modeButtons;
    std::vector<CCMenuItemSpriteExtra*> m_formatButtons;
};

} // namespace paimon::versus
