#include "XPBarNode.hpp"
#include "../data/ProgressionStats.hpp"
#include "../../../utils/SpriteHelper.hpp"
#include "../../../utils/PaimonDrawNode.hpp"

#include <algorithm>
#include <cmath>

using namespace geode::prelude;
using namespace cocos2d;

namespace paimon::progression {

namespace {

constexpr int kGradientSlices = 26;

ccColor4F lerpColor(ccColor3B const& a, ccColor3B const& b, float t, float alpha) {
    return {
        (a.r + (b.r - a.r) * t) / 255.f,
        (a.g + (b.g - a.g) * t) / 255.f,
        (a.b + (b.b - a.b) * t) / 255.f,
        alpha,
    };
}

int64_t lerpExp(int64_t from, int64_t to, float t) {
    return from + static_cast<int64_t>(std::llround((to - from) * static_cast<double>(t)));
}

} // namespace

XPBarNode* XPBarNode::create(float width, float height) {
    auto ret = new XPBarNode();
    if (ret && ret->init(width, height)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool XPBarNode::init(float width, float height) {
    if (!CCNode::init()) return false;

    m_width = width;
    m_height = height;
    this->setContentSize({width, height});
    this->setAnchorPoint({0.5f, 0.5f});

    float const radius = height * 0.5f;

    if (auto* track = paimon::SpriteHelper::createRoundedRect(
            width, height, radius, {0.f, 0.f, 0.f, 0.55f}, {1.f, 1.f, 1.f, 0.14f}, 1.f)) {
        this->addChild(track, 0);
    }

    if (auto* stencil = paimon::SpriteHelper::createRoundedRectStencil(width, height, radius)) {
        if (auto* clip = CCClippingNode::create(stencil)) {
            clip->setAlphaThreshold(0.05f);
            clip->setContentSize({width, height});

            m_fill = PaimonDrawNode::create();
            if (m_fill) clip->addChild(m_fill, 0);

            if (auto* shine = PaimonDrawNode::create()) {
                CCPoint quad[4] = {
                    ccp(0.f, 0.f),
                    ccp(height * 0.7f, 0.f),
                    ccp(height * 1.3f, height),
                    ccp(height * 0.6f, height),
                };
                shine->drawPolygon(quad, 4, {1.f, 1.f, 1.f, 0.28f}, 0.f, {0.f, 0.f, 0.f, 0.f});
                shine->setPosition({-height * 2.f, 0.f});
                shine->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
                shine->runAction(CCRepeatForever::create(CCSequence::create(
                    CCDelayTime::create(1.8f),
                    CCEaseSineInOut::create(CCMoveTo::create(0.85f, ccp(width + height, 0.f))),
                    CCPlace::create(ccp(-height * 2.f, 0.f)),
                    nullptr
                )));
                clip->addChild(shine, 1);
            }
            this->addChild(clip, 1);
        }
    }

    m_label = CCLabelBMFont::create("", "chatFont.fnt");
    if (m_label) {
        m_label->setScale(std::clamp(height * 0.028f, 0.30f, 0.42f));
        m_label->setPosition({width / 2.f, height / 2.f});
        m_label->setColor({235, 242, 255});
        this->addChild(m_label, 2);
    }

    this->scheduleUpdate();
    return true;
}

void XPBarNode::setTier(Tier const& tier) {
    m_base = tier.base;
    m_accent = tier.accent;
    redrawFill();
}

void XPBarNode::setLabelVisible(bool visible) {
    if (m_label) m_label->setVisible(visible);
}

void XPBarNode::setLevelUpCallback(std::function<void(int)> callback) {
    m_onLevelUp = std::move(callback);
}

void XPBarNode::setExp(int64_t exp) {
    m_queue.clear();
    m_segment = 0;
    m_segmentTime = 0.f;
    m_exp = exp;
    applyExp(exp, levelForExp(exp));
}

void XPBarNode::animateTo(int64_t exp, float duration) {
    if (duration <= 0.f || exp <= m_exp) {
        setExp(exp);
        return;
    }

    int64_t const from = m_exp;
    m_queue.clear();
    m_segment = 0;
    m_segmentTime = 0.f;

    // One segment per level crossed, weighted by how much of the gain falls in
    // each so a long grind doesn't spend the whole animation on one level.
    int64_t cursor = from;
    int64_t const span = exp - from;
    while (cursor < exp) {
        int const level = levelForExp(cursor);
        int64_t const boundary = expForLevel(level + 1);
        int64_t const stop = (level >= kMaxLevel) ? exp : std::min(exp, boundary);
        float const share = span > 0 ? static_cast<float>(stop - cursor) / static_cast<float>(span) : 1.f;
        m_queue.push_back({cursor, stop, std::max(0.18f, duration * share)});
        cursor = stop;
        if (m_queue.size() > 12) break;
    }
    if (m_queue.empty()) setExp(exp);
}

void XPBarNode::applyExp(int64_t exp, int level) {
    int64_t const base = expForLevel(level);
    int64_t const span = expSpanOfLevel(level);
    m_shown = span > 0
        ? std::clamp(static_cast<float>(exp - base) / static_cast<float>(span), 0.f, 1.f)
        : 1.f;

    if (m_label) {
        m_label->setString(fmt::format(
            "{} / {} XP",
            formatCount(exp - base),
            span > 0 ? formatCount(span) : std::string("MAX")
        ).c_str());
    }
    redrawFill();
}

void XPBarNode::update(float dt) {
    if (m_segment >= m_queue.size()) return;

    auto const& segment = m_queue[m_segment];
    m_segmentTime = std::min(m_segmentTime + dt, segment.duration);
    float const t = segment.duration > 0.f ? m_segmentTime / segment.duration : 1.f;
    float const eased = 1.f - std::pow(1.f - t, 3.f);

    int const level = levelForExp(segment.from);
    int64_t const current = lerpExp(segment.from, segment.to, eased);
    applyExp(current, level);
    m_exp = current;

    if (m_segmentTime < segment.duration) return;

    ++m_segment;
    m_segmentTime = 0.f;
    m_exp = segment.to;

    int const reached = levelForExp(segment.to);
    if (reached > level) {
        flashLevelUp();
        if (m_onLevelUp) m_onLevelUp(reached);
    }
    if (m_segment >= m_queue.size()) applyExp(m_exp, reached);
}

void XPBarNode::flashLevelUp() {
    if (auto* flash = paimon::SpriteHelper::createRoundedRect(
            m_width, m_height, m_height * 0.5f, {1.f, 1.f, 1.f, 0.85f})) {
        flash->setBlendFunc({GL_SRC_ALPHA, GL_ONE});
        flash->runAction(CCSequence::create(
            CCFadeTo::create(0.35f, 0),
            CCRemoveSelf::create(),
            nullptr
        ));
        this->addChild(flash, 3);
    }
}

void XPBarNode::redrawFill() {
    if (!m_fill) return;
    m_fill->clear();

    float const filled = std::clamp(m_shown, 0.f, 1.f) * m_width;
    if (filled <= 0.5f) return;

    float const slice = m_width / kGradientSlices;
    for (int i = 0; i < kGradientSlices; ++i) {
        float const x0 = slice * i;
        if (x0 >= filled) break;
        float const x1 = std::min(x0 + slice + 0.5f, filled);
        float const t = (static_cast<float>(i) + 0.5f) / kGradientSlices;
        CCPoint quad[4] = {
            ccp(x0, 0.f), ccp(x1, 0.f), ccp(x1, m_height), ccp(x0, m_height),
        };
        m_fill->drawPolygon(quad, 4, lerpColor(m_base, m_accent, t, 1.f), 0.f, {0.f, 0.f, 0.f, 0.f});
    }

    CCPoint cap[4] = {
        ccp(0.f, m_height * 0.58f),
        ccp(filled, m_height * 0.58f),
        ccp(filled, m_height * 0.92f),
        ccp(0.f, m_height * 0.92f),
    };
    m_fill->drawPolygon(cap, 4, {1.f, 1.f, 1.f, 0.16f}, 0.f, {0.f, 0.f, 0.f, 0.f});
}

} // namespace paimon::progression
