#pragma once

// Editing a stored template: which pieces stay, how often each one shows up,
// where it is allowed to sit and which colour channels it paints with.

#include <Geode/Geode.hpp>
#include <Geode/binding/SetTextPopupDelegate.hpp>
#include <Geode/ui/Popup.hpp>

#include <functional>
#include <string>
#include <vector>

#include "../AutobuildTypes.hpp"

namespace paimon::autobuild {

class TemplateEditorPopup : public geode::Popup, public SetTextPopupDelegate {
public:
    static TemplateEditorPopup* create(int index);

    void setTextPopupClosed(SetTextPopup* popup, gd::string text) override;

private:
    bool init() override;
    void onClose(cocos2d::CCObject* sender) override;
    bool setup(int index);
    void rebuild();
    void scheduleRebuild();

    std::vector<cocos2d::CCNode*> templateTab(float width, float inner);
    std::vector<cocos2d::CCNode*> piecesTab(float width, float inner);
    std::vector<cocos2d::CCNode*> colorsTab(float width, float inner);

    cocos2d::CCNode* pieceRow(float width, int index);
    cocos2d::CCNode* pieceDetail(float width);
    cocos2d::CCNode* sidesGrid(float width);
    cocos2d::CCNode* stepperRow(float width, char const* title, char const* desc,
                                int value, int low, int high,
                                std::function<void(int)> onChange);
    void buildActions();
    void addAction(char const* text, char const* sprite, bool enabled,
                   std::function<void()> onPress);

    void save();
    void touch(std::string status);
    void setStatus(std::string text, cocos2d::ccColor3B color);
    void showHelp();

    int m_index = -1;
    int m_tab = 0;
    int m_piece = 0;
    int m_shift = 0;
    int m_remapFrom = 0;  // channel the text popup is about to remap, 0 = renaming
    bool m_dirty = false;
    Template m_draft;
    std::string m_status;
    cocos2d::ccColor3B m_statusColor = {166, 176, 198};
    cocos2d::CCNode* m_content = nullptr;
    cocos2d::CCMenu* m_actions = nullptr;
};

} // namespace paimon::autobuild
