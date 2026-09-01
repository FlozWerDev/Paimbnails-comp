#pragma once

// Analyze a level by id and turn what it finds into templates.

#include <Geode/Geode.hpp>
#include <Geode/binding/SetTextPopupDelegate.hpp>
#include <Geode/ui/Popup.hpp>

#include <memory>
#include <string>
#include <vector>

#include "../services/LevelImport.hpp"

namespace paimon::autobuild {

class LevelAnalysisPopup : public geode::Popup, public SetTextPopupDelegate {
public:
    static LevelAnalysisPopup* create();

    void setTextPopupClosed(SetTextPopup* popup, gd::string text) override;

private:
    bool init() override;
    void onClose(cocos2d::CCObject* sender) override;
    void rebuild();
    void scheduleRebuild();

    std::vector<cocos2d::CCNode*> levelTab(float width, float inner);
    std::vector<cocos2d::CCNode*> piecesTab(float width, float inner);
    std::vector<cocos2d::CCNode*> paletteTab(float width, float inner);

    cocos2d::CCNode* suggestionRow(float width, int index);
    cocos2d::CCNode* paletteRow(float width, int index);
    void buildActions();
    void addAction(char const* text, char const* sprite, bool enabled,
                   std::function<void()> onPress);

    void askLevelId();
    void startAnalysis(int levelId);
    void analyzeOpen();
    void importSuggestion(int index);
    void importAll();
    void setStatus(std::string text, cocos2d::ccColor3B color);
    void showHelp();

    int m_tab = 0;
    int m_lastId = 0;
    bool m_running = false;
    std::string m_status;
    cocos2d::ccColor3B m_statusColor = {166, 176, 198};
    std::shared_ptr<LevelData> m_data;
    LevelReport m_report;
    std::vector<TemplateSuggestion> m_suggestions;
    std::vector<char> m_imported;
    cocos2d::CCNode* m_content = nullptr;
    cocos2d::CCMenu* m_actions = nullptr;
};

} // namespace paimon::autobuild
