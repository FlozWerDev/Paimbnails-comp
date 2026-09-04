#include "BadgeDetailPopup.hpp"
#include "BadgeIconNode.hpp"
#include "../data/ProgressionStats.hpp"
#include "../../../utils/DynamicPopupRegistry.hpp"
#include "../../../utils/Localization.hpp"
#include "../../../utils/SpriteHelper.hpp"

#include <algorithm>

using namespace geode::prelude;
using namespace cocos2d;

namespace paimon::progression {

BadgeDetailPopup* BadgeDetailPopup::create(BadgeDef const& badge, BadgeContext const& ctx) {
    auto ret = new BadgeDetailPopup();
    if (ret && ret->init(badge, ctx)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool BadgeDetailPopup::init(BadgeDef const& badge, BadgeContext const& ctx) {
    if (!Popup::init(300.f, 200.f)) return false;

    auto& loc = Localization::get();
    auto const size = m_mainLayer->getContentSize();
    float const cx = size.width / 2.f;
    bool const unlocked = isUnlocked(badge, ctx);
    auto const accent = rarityColor(badge.rarity);

    this->setTitle(badge.name);
    if (m_title) m_title->setColor(accent);

    if (auto* icon = BadgeIconNode::create(badge, ctx, 76.f)) {
        icon->setPosition({cx, size.height - 92.f});
        icon->playIntro(0.05f);
        m_mainLayer->addChild(icon);
    }

    auto* rarity = CCLabelBMFont::create(
        fmt::format("{}  -  {}", rarityLabel(badge.rarity), categoryLabel(badge.category)).c_str(),
        "chatFont.fnt"
    );
    rarity->setScale(0.44f);
    rarity->setColor(accent);
    rarity->setPosition({cx, size.height - 138.f});
    m_mainLayer->addChild(rarity);

    auto* requirement = CCLabelBMFont::create(badgeRequirement(badge).c_str(), "bigFont.fnt");
    requirement->limitLabelWidth(size.width - 46.f, 0.42f, 0.24f);
    requirement->setPosition({cx, size.height - 160.f});
    m_mainLayer->addChild(requirement);

    float const barW = size.width - 70.f;
    float const barH = 11.f;
    float const barY = 34.f;
    float const progress = badgeProgress(badge, ctx);

    if (auto* track = paimon::SpriteHelper::createRoundedRect(
            barW, barH, barH * 0.5f, {0.f, 0.f, 0.f, 0.55f}, {1.f, 1.f, 1.f, 0.14f}, 1.f)) {
        track->setPosition({cx - barW / 2.f, barY});
        m_mainLayer->addChild(track);
    }

    if (progress > 0.01f) {
        if (auto* fill = paimon::SpriteHelper::createRoundedRect(
                barW * progress, barH, barH * 0.5f,
                {accent.r / 255.f, accent.g / 255.f, accent.b / 255.f, 1.f})) {
            fill->setPosition({cx - barW / 2.f, barY});
            fill->setScaleX(0.f);
            fill->setAnchorPoint({0.f, 0.f});
            fill->runAction(CCSequence::create(
                CCDelayTime::create(0.12f),
                CCEaseSineOut::create(CCScaleTo::create(0.45f, 1.f, 1.f)),
                nullptr
            ));
            m_mainLayer->addChild(fill);
        }
    }

    auto* progressLabel = CCLabelBMFont::create(badgeProgressText(badge, ctx).c_str(), "chatFont.fnt");
    progressLabel->setScale(0.42f);
    progressLabel->setPosition({cx, barY + barH / 2.f});
    progressLabel->setColor({240, 245, 255});
    m_mainLayer->addChild(progressLabel, 2);

    auto* state = CCLabelBMFont::create(
        loc.getString(unlocked ? "progression.badge.unlocked" : "progression.badge.locked").c_str(),
        "goldFont.fnt"
    );
    state->setScale(0.4f);
    state->setPosition({cx, barY + 26.f});
    if (!unlocked) state->setColor({150, 155, 170});
    m_mainLayer->addChild(state);

    paimon::markDynamicPopup(this);
    return true;
}

} // namespace paimon::progression
