#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/ProfilePage.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>
#include <Geode/binding/GJUserScore.hpp>

#include "../data/ProgressionStats.hpp"
#include "../data/ProgressionTiers.hpp"
#include "../services/ProgressionService.hpp"
#include "../ui/ProgressionPopup.hpp"
#include "../ui/TierBadgeNode.hpp"
#include "../../../framework/HookConventions.hpp"
#include "../../../utils/SpriteHelper.hpp"

using namespace geode::prelude;
using namespace paimon::progression;

namespace {

// One chip per profile: mini tier badge plus the level, sized to sit in the
// username row next to the role badges.
CCNode* buildLevelChip(BadgeContext const& ctx, float height) {
    auto const& tier = tierForLevel(ctx.level);

    auto* text = CCLabelBMFont::create(fmt::format("Lv {}", ctx.level).c_str(), "bigFont.fnt");
    if (!text) return nullptr;
    float const textScale = (height * 0.46f) / std::max(1.f, text->getContentSize().height);
    text->setScale(textScale);

    float const badgeSize = height * 0.86f;
    float const width = badgeSize + text->getScaledContentSize().width + height * 0.46f;

    auto* chip = CCNode::create();
    chip->setContentSize({width, height});
    chip->setAnchorPoint({0.5f, 0.5f});

    if (auto* panel = paimon::SpriteHelper::createRoundedRect(
            width, height, height * 0.42f,
            {tier.base.r / 255.f * 0.30f, tier.base.g / 255.f * 0.30f, tier.base.b / 255.f * 0.30f, 0.85f},
            {tier.accent.r / 255.f, tier.accent.g / 255.f, tier.accent.b / 255.f, 0.9f},
            1.f)) {
        chip->addChild(panel, 0);
    }

    if (auto* badge = TierBadgeNode::create(ctx.level, badgeSize)) {
        badge->setPosition({height * 0.14f + badgeSize / 2.f, height / 2.f});
        chip->addChild(badge, 1);
    }

    text->setAnchorPoint({0.f, 0.5f});
    text->setPosition({height * 0.20f + badgeSize, height / 2.f});
    text->setColor(tier.accent);
    chip->addChild(text, 2);

    return chip;
}

} // namespace

class $modify(ProgressionProfilePage, ProfilePage) {
    static void onModify(auto& self) {
        // After the redesign so the username menu is already in its final shape.
        paimon::hooks::afterAllPaimonUiOrVeryLate(self, "ProfilePage::loadPageFromUserInfo");
    }

    struct Fields {
        BadgeContext m_ctx;
        std::string m_username;
    };

    $override
    void loadPageFromUserInfo(GJUserScore* score) {
        ProfilePage::loadPageFromUserInfo(score);

        auto& service = ProgressionService::get();
        if (!service.enabled()) return;

        if (this->m_ownProfile) {
            service.rememberOwnScore(score);
            m_fields->m_ctx = service.ownContext();
        } else {
            if (!score) return;
            m_fields->m_ctx = makeContext(statsFromScore(score));
        }
        if (score) m_fields->m_username = score->m_userName;

        applyLevelChip();
    }

    void applyLevelChip() {
        auto* menu = typeinfo_cast<CCMenu*>(this->getChildByIDRecursive("username-menu"));
        if (!menu) return;

        // The redesign relocates the chip into its own header, so a stale one
        // is not necessarily still under username-menu.
        std::string const id = "paimon-level-badge"_spr;
        while (auto* existing = this->getChildByIDRecursive(id)) {
            existing->removeFromParent();
        }

        auto* chip = buildLevelChip(m_fields->m_ctx, 20.f);
        if (!chip) return;

        auto* btn = CCMenuItemSpriteExtra::create(
            chip, this, menu_selector(ProgressionProfilePage::onLevelChip));
        if (!btn) return;
        btn->setID(id);
        menu->addChild(btn);
        menu->updateLayout();
    }

    void onLevelChip(CCObject*) {
        if (auto* popup = ProgressionPopup::create(m_fields->m_ctx, m_fields->m_username)) {
            popup->show();
        }
    }
};

// The very first snapshot has to be taken before any level is beaten, or the
// next completion would report a whole account's worth of XP as one gain.
class $modify(ProgressionMenuLayer, MenuLayer) {
    $override
    bool init() {
        if (!MenuLayer::init()) return false;

        auto& service = ProgressionService::get();
        if (service.enabled() && !service.hasSnapshot()) {
            service.commitSnapshot();
        }
        return true;
    }
};
