#pragma once

#include <Geode/Geode.hpp>
#include <Geode/cocos/extensions/GUI/CCControlExtension/CCScale9Sprite.h>
#include <string>

namespace paimon::progression {

// GJ_progressBar_001.png as a dark groove plus a tinted scale9 fill, which is
// how the game draws its own bars: the caps stay round at any width and the
// only per-frame work is a content size change.
class GDProgressBar : public cocos2d::CCNode {
public:
    static GDProgressBar* create(float width, float height);

    // The bar texture on its own, cap insets already set, for the places that
    // want a plain GD capsule instead of a filled bar.
    static cocos2d::extension::CCScale9Sprite* makeCapsule();

    void setFillColor(cocos2d::ccColor3B color);
    void setProgress(float progress);
    void animateTo(float progress, float delay, float duration);

    // Centred caption, created on first use.
    void setText(std::string const& text, float scale, cocos2d::ccColor3B color);

    void update(float dt) override;

protected:
    bool init(float width, float height);
    void applyProgress(float progress);

    float m_width = 100.f;
    float m_artHeight = 20.f;
    float m_squash = 1.f;
    float m_from = 0.f;
    float m_target = 0.f;
    float m_delay = 0.f;
    float m_elapsed = 0.f;
    float m_duration = 0.f;
    cocos2d::extension::CCScale9Sprite* m_fill = nullptr;
    cocos2d::CCLabelBMFont* m_label = nullptr;
};

} // namespace paimon::progression
