#pragma once

#include <Geode/Geode.hpp>
#include "../data/ProgressionTiers.hpp"
#include <vector>

namespace paimon::progression {

// Procedurally drawn tier badge: frame silhouette, level number, optional
// progress ring and the per-tier effects (glow, sweep, pulse, sparks, orbit).
// No art ships with it, so a new tier only costs a row in the tier table.
class TierBadgeNode : public cocos2d::CCNode {
public:
    static TierBadgeNode* create(int level, float size);

    void setLevel(int level);
    // Ring around the frame. Negative hides it.
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
    cocos2d::CCDrawNode* m_ring = nullptr;
    cocos2d::CCLabelBMFont* m_levelLabel = nullptr;
};

// Soft round glow drawn as alpha-falloff rings, already set to additive blend.
cocos2d::CCDrawNode* makeRadialGlow(cocos2d::ccColor3B color, float radius, float peakAlpha);

// Outline of a frame shape in points, centred on the origin.
std::vector<cocos2d::CCPoint> frameOutline(TierFrame frame, float radius);
cocos2d::CCPoint frameFanCenter(TierFrame frame, float radius);

// Fills any star-shaped polygon by fanning triangles from `center`, which
// CCDrawNode::drawPolygon cannot do on its own (it fans from vertex 0).
void fillOutline(
    cocos2d::CCDrawNode* node,
    std::vector<cocos2d::CCPoint> const& outline,
    cocos2d::CCPoint const& center,
    cocos2d::ccColor4F const& color
);

} // namespace paimon::progression
