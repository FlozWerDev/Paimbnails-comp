#pragma once

#include <Geode/Geode.hpp>
#include "../data/ProgressionTiers.hpp"

namespace paimon::progression {

// Tier medal: one of the paim_progTier* plates tinted with the tier accent, the
// level number on top and an optional progress ring around it. The per-tier
// effects (glow, shine, pulse, sparks, orbit) are all sprites, so a new tier
// still only costs a row in the tier table.
class TierBadgeNode : public cocos2d::CCNode {
public:
    static TierBadgeNode* create(int level, float size);

    void setLevel(int level);
    // Ring around the medal. Negative hides it.
    void setProgress(float progress);
    void playIntro(float delay);
    void playLevelUp();
    void startPulse();

    int level() const { return m_level; }

protected:
    bool init(int level, float size);
    void rebuild();
    void buildFrame(Tier const& tier);
    void buildEffects(Tier const& tier);
    void redrawRing(Tier const& tier);

    int m_level = 1;
    float m_size = 40.f;
    float m_progress = -1.f;
    bool m_pulses = false;
    cocos2d::CCNode* m_content = nullptr;
    cocos2d::CCNode* m_ring = nullptr;
    cocos2d::CCProgressTimer* m_ringFill = nullptr;
    cocos2d::CCLabelBMFont* m_levelLabel = nullptr;
};

// Tinted glow sprite, already set to additive blend.
cocos2d::CCSprite* makeRadialGlow(cocos2d::ccColor3B color, float radius, float peakAlpha);

// Medal plate for a tier, unscaled and untinted.
cocos2d::CCSprite* makeTierPlate(TierFrame frame);

} // namespace paimon::progression
