#pragma once

#include <Geode/ui/Popup.hpp>
#include <Geode/ui/LazySprite.hpp>
#include <Geode/utils/cocos.hpp>
#include <map>
#include <string>

// Fullscreen viewer for mod previews with prev/next navigation.
// Inspired by Alphalaneous's Mod-Previews.

namespace paimon::mod_previews {

class ModPreviewGalleryPopup : public geode::Popup {
public:
    // page: initial image (1-based). count: total previews. urlBase: prefix without "{n}.png".
    static ModPreviewGalleryPopup* create(int page, int count, std::string urlBase);

protected:
    int m_page = 1;
    int m_count = 1;
    std::string m_urlBase;
    geode::LazySprite* m_current = nullptr;
    cocos2d::CCLabelBMFont* m_label = nullptr;
    std::map<int, geode::Ref<geode::LazySprite>> m_cache;

    bool init(int page, int count, std::string urlBase);
    void showImage(int page);
    void onLoad(geode::LazySprite* spr);
    void onPrev(cocos2d::CCObject*);
    void onNext(cocos2d::CCObject*);
};

} // namespace paimon::mod_previews
