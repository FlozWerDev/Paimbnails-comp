#include "TierBadgeNode.hpp"
#include "../../../utils/SpriteHelper.hpp"
#include "../../../utils/PaimonDrawNode.hpp"

#include <algorithm>
#include <cmath>

using namespace geode::prelude;
using namespace cocos2d;

namespace paimon::progression {

namespace {

constexpr float kPi = 3.14159265358979323846f;

ccColor4F toColor4F(ccColor3B const& c, float alpha) {
    return {c.r / 255.f, c.g / 255.f, c.b / 255.f, alpha};
}

ccColor3B mix(ccColor3B const& a, ccColor3B const& b, float t) {
    return {
        static_cast<GLubyte>(a.r + (b.r - a.r) * t),
        static_cast<GLubyte>(a.g + (b.g - a.g) * t),
        static_cast<GLubyte>(a.b + (b.b - a.b) * t),
    };
}

void appendArc(std::vector<CCPoint>& out, float cx, float cy, float radius,
               float fromAngle, float toAngle, int segments) {
    for (int i = 0; i <= segments; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(segments);
        float const a = fromAngle + (toAngle - fromAngle) * t;
        out.push_back(ccp(cx + std::cos(a) * radius, cy + std::sin(a) * radius));
    }
}

void appendQuadratic(std::vector<CCPoint>& out, CCPoint a, CCPoint c, CCPoint b, int segments) {
    for (int i = 1; i <= segments; ++i) {
        float const t = static_cast<float>(i) / static_cast<float>(segments);
        float const u = 1.f - t;
        out.push_back(ccp(
            u * u * a.x + 2.f * u * t * c.x + t * t * b.x,
            u * u * a.y + 2.f * u * t * c.y + t * t * b.y
        ));
    }
}

// A soft horizontal light bar faked with alpha-ramped slices, since CCDrawNode
// only fills flat colours.
CCDrawNode* buildSweepBar(float width, float height) {
    auto* node = PaimonDrawNode::create();
    if (!node) return nullptr;

    constexpr int kSlices = 9;
    float const slice = width / kSlices;
    for (int i = 0; i < kSlices; ++i) {
        float const t = (static_cast<float>(i) + 0.5f) / kSlices;
        float const alpha = std::sin(t * kPi) * 0.42f;
        float const x = -width / 2.f + slice * i;
        CCPoint quad[4] = {
            ccp(x, -height / 2.f),
            ccp(x + slice + 0.5f, -height / 2.f),
            ccp(x + slice + 0.5f + height * 0.35f, height / 2.f),
            ccp(x + height * 0.35f, height / 2.f),
        };
        node->drawPolygon(quad, 4, {1.f, 1.f, 1.f, alpha}, 0.f, {0.f, 0.f, 0.f, 0.f});
    }
    return node;
}

} // namespace

CCDrawNode* makeRadialGlow(ccColor3B color, float radius, float peakAlpha) {
    auto* node = PaimonDrawNode::create();
    if (!node) return nullptr;

    constexpr int kRings = 10;
    constexpr int kSegments = 18;
    float const step = radius / kRings;

    for (int ring = 0; ring < kRings; ++ring) {
        float const inner = step * ring;
        float const outer = step * (ring + 1);
        float const t = (static_cast<float>(ring) + 0.5f) / kRings;
        ccColor4F const fill = {
            color.r / 255.f, color.g / 255.f, color.b / 255.f,
            peakAlpha * std::pow(1.f - t, 2.2f),
        };

        for (int i = 0; i < kSegments; ++i) {
            float const a0 = static_cast<float>(i) * kPi * 2.f / kSegments;
            float const a1 = static_cast<float>(i + 1) * kPi * 2.f / kSegments;
            if (ring == 0) {
                CCPoint tri[3] = {
                    ccp(0.f, 0.f),
                    ccp(std::cos(a0) * outer, std::sin(a0) * outer),
                    ccp(std::cos(a1) * outer, std::sin(a1) * outer),
                };
                node->drawPolygon(tri, 3, fill, 0.f, {0.f, 0.f, 0.f, 0.f});
                continue;
            }
            CCPoint quad[4] = {
                ccp(std::cos(a0) * inner, std::sin(a0) * inner),
                ccp(std::cos(a0) * outer, std::sin(a0) * outer),
                ccp(std::cos(a1) * outer, std::sin(a1) * outer),
                ccp(std::cos(a1) * inner, std::sin(a1) * inner),
            };
            node->drawPolygon(quad, 4, fill, 0.f, {0.f, 0.f, 0.f, 0.f});
        }
    }

    node->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
    return node;
}

std::vector<CCPoint> frameOutline(TierFrame frame, float r) {
    std::vector<CCPoint> out;
    switch (frame) {
        case TierFrame::Pill: {
            appendArc(out, 0.f, 0.f, r, 0.f, kPi * 2.f, 30);
            out.pop_back();
            break;
        }
        case TierFrame::Hexagon: {
            for (int i = 0; i < 6; ++i) {
                float const a = kPi * 0.5f + static_cast<float>(i) * kPi / 3.f;
                out.push_back(ccp(std::cos(a) * r, std::sin(a) * r));
            }
            break;
        }
        case TierFrame::Star: {
            for (int i = 0; i < 10; ++i) {
                float const a = kPi * 0.5f + static_cast<float>(i) * kPi / 5.f;
                float const rad = (i % 2 == 0) ? r : r * 0.46f;
                out.push_back(ccp(std::cos(a) * rad, std::sin(a) * rad));
            }
            break;
        }
        case TierFrame::Shield: {
            out.push_back(ccp(-0.86f * r, 0.50f * r));
            appendQuadratic(out, ccp(-0.86f * r, 0.50f * r), ccp(0.f, 0.92f * r), ccp(0.86f * r, 0.50f * r), 8);
            out.push_back(ccp(0.86f * r, -0.10f * r));
            appendQuadratic(out, ccp(0.86f * r, -0.10f * r), ccp(0.68f * r, -0.68f * r), ccp(0.f, -0.98f * r), 8);
            appendQuadratic(out, ccp(0.f, -0.98f * r), ccp(-0.68f * r, -0.68f * r), ccp(-0.86f * r, -0.10f * r), 8);
            break;
        }
        case TierFrame::Crown: {
            out.push_back(ccp(-0.94f * r, -0.62f * r));
            out.push_back(ccp( 0.94f * r, -0.62f * r));
            out.push_back(ccp( 0.94f * r,  0.34f * r));
            out.push_back(ccp( 0.50f * r, -0.06f * r));
            out.push_back(ccp( 0.00f * r,  0.90f * r));
            out.push_back(ccp(-0.50f * r, -0.06f * r));
            out.push_back(ccp(-0.94f * r,  0.34f * r));
            break;
        }
    }
    return out;
}

CCPoint frameFanCenter(TierFrame frame, float r) {
    // The crown's valleys sit on the centroid, which would make degenerate
    // triangles; fan from inside the base instead.
    if (frame == TierFrame::Crown) return ccp(0.f, -0.45f * r);
    return ccp(0.f, 0.f);
}

void fillOutline(CCDrawNode* node, std::vector<CCPoint> const& outline,
                 CCPoint const& center, ccColor4F const& color) {
    if (!node || outline.size() < 3) return;

    size_t const count = outline.size();
    for (size_t i = 0; i < count; ++i) {
        CCPoint tri[3] = {center, outline[i], outline[(i + 1) % count]};
        node->drawPolygon(tri, 3, color, 0.f, {0.f, 0.f, 0.f, 0.f});
    }
}

TierBadgeNode* TierBadgeNode::create(int level, float size) {
    auto ret = new TierBadgeNode();
    if (ret && ret->init(level, size)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool TierBadgeNode::init(int level, float size) {
    if (!CCNode::init()) return false;

    m_level = level;
    m_size = size;
    this->setContentSize({size, size});
    this->setAnchorPoint({0.5f, 0.5f});

    m_content = CCNode::create();
    m_content->setPosition({size / 2.f, size / 2.f});
    this->addChild(m_content);

    rebuild();
    return true;
}

void TierBadgeNode::setLevel(int level) {
    if (level == m_level) return;
    m_level = level;
    rebuild();
}

void TierBadgeNode::setProgress(float progress) {
    m_progress = progress;
    redrawRing(tierForLevel(m_level));
}

void TierBadgeNode::rebuild() {
    m_content->removeAllChildren();
    m_ring = nullptr;
    m_levelLabel = nullptr;

    auto const& tier = tierForLevel(m_level);
    buildFrame(tier);
    buildEffects(tier);
    redrawRing(tier);
}

void TierBadgeNode::buildFrame(Tier const& tier) {
    float const r = m_size * 0.5f;
    auto const outline = frameOutline(tier.frame, r * 0.86f);
    auto const center = frameFanCenter(tier.frame, r * 0.86f);

    if (tier.effects & TierEffectGlow) {
        if (auto* glow = makeRadialGlow(tier.accent, m_size * 0.95f, 0.5f)) {
            glow->setScale(0.92f);
            glow->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCScaleTo::create(1.6f, 1.08f)),
                CCEaseSineInOut::create(CCScaleTo::create(1.6f, 0.92f)),
                nullptr
            )));
            m_content->addChild(glow, -2);
        }
    }

    // Outer rim slightly larger than the fill, so the accent reads as a border.
    if (auto* rim = PaimonDrawNode::create()) {
        auto const rimOutline = frameOutline(tier.frame, r * 0.94f);
        fillOutline(rim, rimOutline, frameFanCenter(tier.frame, r * 0.94f),
                    toColor4F(tier.accent, 0.95f));
        m_content->addChild(rim, -1);
    }

    if (auto* body = PaimonDrawNode::create()) {
        fillOutline(body, outline, center, toColor4F(tier.base, 1.f));
        // A smaller concentric silhouette reads as a lit core and, unlike a
        // half-outline, can never poke outside a concave frame.
        float const coreR = r * 0.86f * 0.64f;
        fillOutline(body, frameOutline(tier.frame, coreR), frameFanCenter(tier.frame, coreR),
                    toColor4F(mix(tier.base, tier.accent, 0.38f), 0.5f));
        m_content->addChild(body, 0);
    }

    // At chip size the number is unreadable and the chip prints it anyway.
    if (m_size < 26.f) return;

    m_levelLabel = CCLabelBMFont::create(std::to_string(m_level).c_str(), "bigFont.fnt");
    if (m_levelLabel) {
        float const target = m_size * (m_level >= 100 ? 0.34f : 0.42f);
        m_levelLabel->limitLabelWidth(m_size * 0.62f, target / std::max(1.f, m_levelLabel->getContentSize().height), 0.05f);
        m_levelLabel->setPosition({0.f, tier.frame == TierFrame::Crown ? -m_size * 0.06f : 0.f});
        m_levelLabel->setColor(mix(tier.accent, {255, 255, 255}, 0.55f));
        m_content->addChild(m_levelLabel, 3);
    }
}

void TierBadgeNode::buildEffects(Tier const& tier) {
    float const r = m_size * 0.5f;

    if (tier.effects & TierEffectSweep) {
        auto const outline = frameOutline(tier.frame, r * 0.86f);
        auto* stencil = PaimonDrawNode::create();
        if (stencil) {
            fillOutline(stencil, outline, frameFanCenter(tier.frame, r * 0.86f), {1.f, 1.f, 1.f, 1.f});
            if (auto* clip = CCClippingNode::create(stencil)) {
                clip->setAlphaThreshold(0.05f);
                if (auto* bar = buildSweepBar(m_size * 0.32f, m_size * 1.7f)) {
                    bar->setPosition({-m_size * 0.9f, 0.f});
                    bar->runAction(CCRepeatForever::create(CCSequence::create(
                        CCDelayTime::create(2.4f),
                        CCEaseSineInOut::create(CCMoveTo::create(0.62f, ccp(m_size * 0.9f, 0.f))),
                        CCPlace::create(ccp(-m_size * 0.9f, 0.f)),
                        nullptr
                    )));
                    clip->addChild(bar);
                }
                m_content->addChild(clip, 2);
            }
        }
    }

    m_pulses = (tier.effects & TierEffectPulse) != 0;
    if (m_pulses) startPulse();

    if (tier.effects & TierEffectSparks) {
        for (int i = 0; i < 5; ++i) {
            auto* spark = paimon::SpriteHelper::safeCreateWithFrameName("GJ_bigStar_noShadow_001.png");
            if (!spark) break;
            float const a = kPi * 0.5f + static_cast<float>(i) * kPi * 0.4f;
            float const sparkUnit = std::max(1.f, spark->getContentSize().width);
            spark->setPosition({std::cos(a) * r * 0.94f, std::sin(a) * r * 0.94f});
            spark->setScale(m_size * 0.10f / sparkUnit);
            spark->setColor(tier.accent);
            spark->setOpacity(0);
            spark->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
            spark->runAction(CCRepeatForever::create(CCSequence::create(
                CCDelayTime::create(0.28f * i),
                CCSpawn::create(
                    CCFadeTo::create(0.30f, 230),
                    CCScaleTo::create(0.30f, m_size * 0.19f / sparkUnit),
                    nullptr
                ),
                CCSpawn::create(
                    CCFadeTo::create(0.42f, 0),
                    CCScaleTo::create(0.42f, m_size * 0.06f / sparkUnit),
                    nullptr
                ),
                CCDelayTime::create(1.5f - 0.28f * i),
                nullptr
            )));
            m_content->addChild(spark, 4);
        }
    }

    if (tier.effects & TierEffectOrbit) {
        auto* orbit = CCNode::create();
        for (int i = 0; i < 2; ++i) {
            auto* dot = paimon::SpriteHelper::safeCreateWithFrameName("GJ_bigStar_noShadow_001.png");
            if (!dot) break;
            dot->setPosition({i == 0 ? r * 1.24f : -r * 1.24f, 0.f});
            dot->setScale(m_size * 0.14f / std::max(1.f, dot->getContentSize().width));
            dot->setColor(mix(tier.accent, {255, 255, 255}, 0.4f));
            dot->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
            orbit->addChild(dot);
        }
        orbit->setScaleY(0.42f);
        orbit->runAction(CCRepeatForever::create(CCRotateBy::create(4.2f, -360.f)));
        m_content->addChild(orbit, 5);
    }
}

void TierBadgeNode::redrawRing(Tier const& tier) {
    if (m_progress < 0.f) {
        if (m_ring) {
            m_ring->removeFromParent();
            m_ring = nullptr;
        }
        return;
    }

    if (!m_ring) {
        m_ring = PaimonDrawNode::create();
        if (!m_ring) return;
        m_content->addChild(m_ring, 1);
    }
    m_ring->clear();

    float const r = m_size * 0.5f;
    float const inner = r * 1.02f;
    float const outer = r * 1.13f;
    constexpr int kSegments = 60;

    auto arc = [&](float from, float to, ccColor4F const& color) {
        int const steps = std::max(1, static_cast<int>(std::ceil((to - from) / (kPi * 2.f) * kSegments)));
        for (int i = 0; i < steps; ++i) {
            float const a0 = from + (to - from) * (static_cast<float>(i) / steps);
            float const a1 = from + (to - from) * (static_cast<float>(i + 1) / steps);
            CCPoint quad[4] = {
                ccp(std::cos(a0) * inner, std::sin(a0) * inner),
                ccp(std::cos(a0) * outer, std::sin(a0) * outer),
                ccp(std::cos(a1) * outer, std::sin(a1) * outer),
                ccp(std::cos(a1) * inner, std::sin(a1) * inner),
            };
            m_ring->drawPolygon(quad, 4, color, 0.f, {0.f, 0.f, 0.f, 0.f});
        }
    };

    arc(0.f, kPi * 2.f, {0.f, 0.f, 0.f, 0.42f});
    float const swept = std::clamp(m_progress, 0.f, 1.f) * kPi * 2.f;
    if (swept > 0.001f) {
        float const start = kPi * 0.5f;
        arc(start - swept, start, toColor4F(tier.accent, 0.95f));
    }
}

void TierBadgeNode::startPulse() {
    if (!m_pulses) return;
    m_content->runAction(CCRepeatForever::create(CCSequence::create(
        CCEaseSineInOut::create(CCScaleTo::create(1.4f, 1.045f)),
        CCEaseSineInOut::create(CCScaleTo::create(1.4f, 1.f)),
        nullptr
    )));
}

void TierBadgeNode::playIntro(float delay) {
    m_content->stopAllActions();
    m_content->setScale(0.f);
    m_content->runAction(CCSequence::create(
        CCDelayTime::create(delay),
        CCEaseBackOut::create(CCScaleTo::create(0.42f, 1.f)),
        CCCallFunc::create(this, callfunc_selector(TierBadgeNode::startPulse)),
        nullptr
    ));
}

void TierBadgeNode::playLevelUp() {
    auto const& tier = tierForLevel(m_level);
    float const r = m_size * 0.5f;

    m_content->stopAllActions();
    m_content->setScale(0.25f);
    m_content->runAction(CCSequence::create(
        CCEaseElasticOut::create(CCScaleTo::create(0.9f, 1.f), 0.55f),
        CCCallFunc::create(this, callfunc_selector(TierBadgeNode::startPulse)),
        nullptr
    ));

    if (auto* flash = PaimonDrawNode::create()) {
        auto const outline = frameOutline(tier.frame, r * 0.94f);
        fillOutline(flash, outline, frameFanCenter(tier.frame, r * 0.94f), {1.f, 1.f, 1.f, 1.f});
        flash->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        flash->runAction(CCSequence::create(
            CCDelayTime::create(0.12f),
            CCFadeTo::create(0.45f, 0),
            CCRemoveSelf::create(),
            nullptr
        ));
        m_content->addChild(flash, 6);
    }

    if (auto* wave = PaimonDrawNode::create()) {
        auto const outline = frameOutline(tier.frame, r * 0.94f);
        fillOutline(wave, outline, frameFanCenter(tier.frame, r * 0.94f), toColor4F(tier.accent, 0.55f));
        wave->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        wave->runAction(CCSpawn::create(
            CCEaseSineOut::create(CCScaleTo::create(0.7f, 2.4f)),
            CCFadeTo::create(0.7f, 0),
            nullptr
        ));
        wave->runAction(CCSequence::create(
            CCDelayTime::create(0.75f),
            CCRemoveSelf::create(),
            nullptr
        ));
        m_content->addChild(wave, -3);
    }
}

} // namespace paimon::progression
