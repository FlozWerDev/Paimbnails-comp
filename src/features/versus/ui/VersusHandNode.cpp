#include "VersusHandNode.hpp"
#include "VersusCardNode.hpp"
#include "../services/VersusEffects.hpp"
#include "../services/VersusSession.hpp"
#include "../../../utils/Localization.hpp"
#include "../../../utils/SpriteHelper.hpp"

#include <algorithm>

using namespace geode::prelude;
using namespace cocos2d;

namespace paimon::versus {

namespace {

constexpr float kCardW = 46.f;
constexpr float kSlotGap = 54.f;
constexpr float kEffectSize = 22.f;

} // namespace

VersusHandNode* VersusHandNode::create() {
    auto ret = new VersusHandNode();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool VersusHandNode::init() {
    if (!CCNode::init()) return false;

    auto const winSize = CCDirector::get()->getWinSize();
    this->setPosition({0.f, 0.f});

    m_slots = CCNode::create();
    m_slots->setPosition({winSize.width - 46.f, 52.f});
    this->addChild(m_slots, 1);

    m_effects = CCNode::create();
    m_effects->setPosition({winSize.width - 16.f, 110.f});
    this->addChild(m_effects, 1);

    m_locked = CCLabelBMFont::create(
        Localization::get().getString("versus.hand.locked").c_str(), "chatFont.fnt");
    m_locked->setScale(0.4f);
    m_locked->setAnchorPoint({1.f, 0.5f});
    m_locked->setColor({240, 130, 140});
    m_locked->setPosition({winSize.width - 12.f, 24.f});
    m_locked->setVisible(false);
    this->addChild(m_locked, 2);

    this->scheduleUpdate();
    refresh();
    return true;
}

void VersusHandNode::update(float) {
    refresh();
}

void VersusHandNode::refresh() {
    auto const& hand = VersusSession::get().hand();
    if (hand != m_drawn) {
        m_drawn = hand;
        rebuildHand();
    }

    auto const& active = VersusEffects::get().active();
    if (active.size() != m_effectCount) {
        m_effectCount = active.size();
        rebuildEffects();
    }

    m_locked->setVisible(VersusEffects::get().cardsLocked());
}

void VersusHandNode::rebuildHand() {
    m_slots->removeAllChildren();

    for (size_t i = 0; i < m_drawn.size(); i++) {
        auto* card = VersusCardNode::create(m_drawn[i], kCardW);
        if (!card) continue;
        card->setPosition({-static_cast<float>(i) * kSlotGap, 0.f});
        card->playDraw(0.f);
        m_slots->addChild(card, static_cast<int>(10 - i));

        auto* key = CCLabelBMFont::create(i == 0 ? "Q" : "E", "bigFont.fnt");
        key->setScale(0.34f);
        key->setOpacity(190);
        key->setPosition({-static_cast<float>(i) * kSlotGap, -kCardW * 0.86f});
        m_slots->addChild(key, 20);
    }
}

void VersusHandNode::rebuildEffects() {
    m_effects->removeAllChildren();

    auto const& active = VersusEffects::get().active();
    for (size_t i = 0; i < active.size(); i++) {
        auto const& effect = active[i];
        auto const& def = cardAt(effect.card);

        auto* glyph = paimon::SpriteHelper::safeCreate(cardGlyphSprite(def).c_str());
        if (!glyph) continue;
        glyph->setScale(kEffectSize / std::max(1.f, glyph->getContentSize().width));
        glyph->setPosition({0.f, static_cast<float>(i) * (kEffectSize + 6.f)});
        // Incoming cards are the rival's doing, so they read in his colour.
        glyph->setColor(effect.fromRival ? ccColor3B{240, 120, 140} : ccColor3B{140, 220, 250});
        m_effects->addChild(glyph, 1);

        if (effect.total > 0.f) {
            auto* ring = CCProgressTimer::create(
                paimon::SpriteHelper::safeCreate("paim_vsPip.png"_spr));
            if (!ring) continue;
            ring->setType(kCCProgressTimerTypeRadial);
            ring->setReverseDirection(true);
            ring->setScale(kEffectSize * 1.5f / std::max(1.f, ring->getContentSize().width));
            ring->setPosition(glyph->getPosition());
            ring->setOpacity(120);
            m_effects->addChild(ring, 0);
        }
    }
}

} // namespace paimon::versus
