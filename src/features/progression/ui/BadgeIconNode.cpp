#include "BadgeIconNode.hpp"
#include "TierBadgeNode.hpp"
#include "../../../utils/SpriteHelper.hpp"
#include "../../../utils/PaimonDrawNode.hpp"

#include <algorithm>

using namespace geode::prelude;
using namespace cocos2d;

namespace paimon::progression {

namespace {

ccColor4F toColor4F(ccColor3B const& c, float alpha) {
    return {c.r / 255.f, c.g / 255.f, c.b / 255.f, alpha};
}

} // namespace

BadgeIconNode* BadgeIconNode::create(BadgeDef const& badge, BadgeContext const& ctx, float size) {
    auto ret = new BadgeIconNode();
    if (ret && ret->init(badge, ctx, size)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool BadgeIconNode::init(BadgeDef const& badge, BadgeContext const& ctx, float size) {
    if (!CCNode::init()) return false;

    m_badge = &badge;
    m_size = size;
    m_unlocked = isUnlocked(badge, ctx);
    this->setContentSize({size, size});
    this->setAnchorPoint({0.5f, 0.5f});

    m_content = CCNode::create();
    m_content->setPosition({size / 2.f, size / 2.f});
    this->addChild(m_content);

    auto const accent = rarityColor(badge.rarity);
    float const radius = size * 0.24f;

    if (m_unlocked && badge.rarity >= BadgeRarity::Epic) {
        if (auto* glow = makeRadialGlow(accent, size * 0.82f, 0.42f)) {
            glow->setScale(0.9f);
            glow->runAction(CCRepeatForever::create(CCSequence::create(
                CCEaseSineInOut::create(CCScaleTo::create(1.7f, 1.06f)),
                CCEaseSineInOut::create(CCScaleTo::create(1.7f, 0.9f)),
                nullptr
            )));
            m_content->addChild(glow, -1);
        }
    }

    auto const fill = m_unlocked
        ? toColor4F(accent, 0.20f)
        : ccColor4F{0.f, 0.f, 0.f, 0.34f};
    auto const border = m_unlocked
        ? toColor4F(accent, 0.95f)
        : ccColor4F{1.f, 1.f, 1.f, 0.12f};

    if (auto* frame = paimon::SpriteHelper::createRoundedRect(size, size, radius, fill, border, 1.4f)) {
        frame->setPosition({-size / 2.f, -size / 2.f});
        m_content->addChild(frame, 0);
    }

    if (auto* glyph = paimon::SpriteHelper::safeCreateWithFrameName(badge.glyph)) {
        float const target = size * 0.52f;
        float const source = std::max(glyph->getContentSize().width, glyph->getContentSize().height);
        glyph->setScale(target / std::max(1.f, source));
        glyph->setPosition({0.f, m_unlocked ? 0.f : size * 0.04f});
        if (!m_unlocked) {
            glyph->setColor({58, 62, 78});
            glyph->setOpacity(215);
        }
        m_content->addChild(glyph, 1);
    }

    if (!m_unlocked) {
        float const progress = badgeProgress(badge, ctx);
        float const barW = size * 0.62f;
        float const barH = std::max(2.5f, size * 0.07f);
        float const barY = -size * 0.36f;

        if (auto* track = paimon::SpriteHelper::createRoundedRect(
                barW, barH, barH * 0.5f, {0.f, 0.f, 0.f, 0.5f})) {
            track->setPosition({-barW / 2.f, barY});
            m_content->addChild(track, 2);
        }
        if (progress > 0.02f) {
            if (auto* bar = paimon::SpriteHelper::createRoundedRect(
                    barW * progress, barH, barH * 0.5f, toColor4F(accent, 0.85f))) {
                bar->setPosition({-barW / 2.f, barY});
                m_content->addChild(bar, 3);
            }
        }

        if (auto* lock = paimon::SpriteHelper::safeCreateWithFrameName("GJ_lockGray_001.png")) {
            lock->setScale(size * 0.30f / std::max(1.f, lock->getContentSize().height));
            lock->setPosition({size * 0.30f, size * 0.30f});
            lock->setOpacity(190);
            m_content->addChild(lock, 4);
        }
    }

    return true;
}

void BadgeIconNode::playIntro(float delay) {
    m_content->setScale(0.f);
    m_content->runAction(CCSequence::create(
        CCDelayTime::create(delay),
        CCEaseBackOut::create(CCScaleTo::create(0.30f, 1.f)),
        nullptr
    ));
}

void BadgeIconNode::playUnlock() {
    auto const accent = rarityColor(m_badge->rarity);

    m_content->stopAllActions();
    m_content->setScale(0.3f);
    m_content->runAction(CCEaseElasticOut::create(CCScaleTo::create(0.85f, 1.f), 0.6f));

    if (auto* wave = paimon::SpriteHelper::createRoundedRect(
            m_size, m_size, m_size * 0.24f, toColor4F(accent, 0.6f))) {
        wave->setPosition({-m_size / 2.f, -m_size / 2.f});
        wave->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        wave->runAction(CCSpawn::create(
            CCEaseSineOut::create(CCScaleTo::create(0.65f, 2.1f)),
            CCFadeTo::create(0.65f, 0),
            nullptr
        ));
        wave->runAction(CCSequence::create(
            CCDelayTime::create(0.7f),
            CCRemoveSelf::create(),
            nullptr
        ));
        m_content->addChild(wave, -2);
    }
}

} // namespace paimon::progression
