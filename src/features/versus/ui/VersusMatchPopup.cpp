#include "VersusMatchPopup.hpp"
#include "../data/VersusModes.hpp"
#include "../data/VersusRanks.hpp"
#include "../services/VersusSession.hpp"
#include "../../../utils/DynamicPopupRegistry.hpp"
#include "../../../utils/Localization.hpp"
#include "../../../utils/SpriteHelper.hpp"

#include <algorithm>

using namespace geode::prelude;
using namespace cocos2d;

namespace paimon::versus {

namespace {

constexpr float kPopupW = 400.f;
constexpr float kPopupH = 270.f;
constexpr int kOfferTag = 6100;

char const* difficultySprite(int difficulty) {
    switch (std::clamp(difficulty, 1, 6)) {
        case 1:  return "diffIcon_01_btn_001.png";
        case 2:  return "diffIcon_02_btn_001.png";
        case 3:  return "diffIcon_03_btn_001.png";
        case 4:  return "diffIcon_04_btn_001.png";
        case 5:  return "diffIcon_05_btn_001.png";
        default: return "diffIcon_06_btn_001.png";
    }
}

} // namespace

VersusMatchPopup* VersusMatchPopup::create() {
    auto ret = new VersusMatchPopup();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool VersusMatchPopup::init() {
    if (!Popup::init(kPopupW, kPopupH)) return false;

    paimon::markDynamicPopup(this);
    this->setTitle(Localization::get().getString("versus.match.title"));

    m_menu = CCMenu::create();
    m_menu->setPosition({0.f, 0.f});
    m_mainLayer->addChild(m_menu, 5);

    rebuild();
    return true;
}

void VersusMatchPopup::onEnter() {
    Popup::onEnter();

    auto self = Ref<VersusMatchPopup>(this);
    VersusSession::get().addListener(this, [self]() {
        if (self->isRunning()) self->rebuild();
    });
}

void VersusMatchPopup::onExit() {
    VersusSession::get().removeListener(this);
    Popup::onExit();
}

void VersusMatchPopup::rebuild() {
    auto const phase = VersusSession::get().phase();

    if (phase == Phase::Countdown || phase == Phase::Running) {
        // The level takes over from here.
        Popup::onClose(nullptr);
        return;
    }
    if (phase == Phase::Idle || phase == Phase::Finished) {
        Popup::onClose(nullptr);
        return;
    }
    if (phase == m_drawn && m_page) return;

    m_drawn = phase;
    if (m_page) m_page->removeFromParent();
    m_menu->removeAllChildren();

    m_page = CCNode::create();
    m_page->setContentSize({kPopupW, kPopupH});
    m_mainLayer->addChild(m_page, 2);

    switch (phase) {
        case Phase::Found:   buildFound(m_page); break;
        case Phase::Banning: buildBanning(m_page); break;
        default:             buildLoading(m_page); break;
    }
}

void VersusMatchPopup::buildFound(CCNode* page) {
    auto const& match = VersusSession::get().match();
    auto& loc = Localization::get();

    if (auto* burst = paimon::SpriteHelper::safeCreate("paim_vsBurst.png"_spr)) {
        burst->setScale(160.f / std::max(1.f, burst->getContentSize().width));
        burst->setPosition({kPopupW / 2.f, kPopupH - 120.f});
        burst->setColor({255, 226, 140});
        burst->setOpacity(70);
        burst->runAction(CCRepeatForever::create(CCRotateBy::create(18.f, 360.f)));
        page->addChild(burst, 0);
    }

    if (auto* logo = paimon::SpriteHelper::safeCreate("paim_vsLogo.png"_spr)) {
        logo->setScale(34.f / std::max(1.f, logo->getContentSize().height));
        logo->setPosition({kPopupW / 2.f, kPopupH - 120.f});
        logo->setColor({255, 226, 140});
        page->addChild(logo, 3);
    }

    auto const rivalRank = rankFor(match.rival.elo, match.rival.placementsLeft);
    if (auto* badge = VersusRankBadgeNode::create(rivalRank, 62.f)) {
        badge->setPosition({kPopupW * 0.74f, kPopupH - 120.f});
        page->addChild(badge, 2);
    }

    auto* rivalName = CCLabelBMFont::create(match.rival.name.c_str(), "goldFont.fnt");
    rivalName->setScale(0.56f);
    rivalName->setPosition({kPopupW * 0.74f, kPopupH - 162.f});
    page->addChild(rivalName, 2);

    auto* record = CCLabelBMFont::create(
        fmt::format("{} - {}W {}L", rankName(rivalRank), match.rival.wins, match.rival.losses).c_str(),
        "chatFont.fnt");
    record->setScale(0.46f);
    record->setOpacity(190);
    record->setPosition({kPopupW * 0.74f, kPopupH - 180.f});
    page->addChild(record, 2);

    auto const& def = formatAt(match.format);
    auto* format = CCLabelBMFont::create(
        fmt::format("{} - {}", formatName(def), modeId(match.mode)).c_str(), "chatFont.fnt");
    format->setScale(0.5f);
    format->setOpacity(200);
    format->setPosition({kPopupW * 0.28f, kPopupH - 162.f});
    page->addChild(format, 2);

    if (auto* glyph = paimon::SpriteHelper::safeCreate(formatSprite(def).c_str())) {
        glyph->setScale(46.f / std::max(1.f, glyph->getContentSize().width));
        glyph->setPosition({kPopupW * 0.28f, kPopupH - 120.f});
        page->addChild(glyph, 2);
    }

    auto* acceptFace = ButtonSprite::create(loc.getString("versus.match.accept").c_str(), 110, true,
                                            "bigFont.fnt", "GJ_button_01.png", 30.f, 0.6f);
    auto* accept = CCMenuItemSpriteExtra::create(acceptFace, this,
                                                 menu_selector(VersusMatchPopup::onAccept));
    accept->setPosition({kPopupW / 2.f - 66.f, 44.f});
    m_menu->addChild(accept);

    auto* declineFace = ButtonSprite::create(loc.getString("versus.match.decline").c_str(), 100, true,
                                             "bigFont.fnt", "GJ_button_06.png", 30.f, 0.5f);
    auto* decline = CCMenuItemSpriteExtra::create(declineFace, this,
                                                  menu_selector(VersusMatchPopup::onDecline));
    decline->setPosition({kPopupW / 2.f + 66.f, 44.f});
    m_menu->addChild(decline);

    auto* warning = CCLabelBMFont::create(loc.getString("versus.match.dodge-warning").c_str(),
                                          "chatFont.fnt");
    warning->setScale(0.42f);
    warning->setOpacity(150);
    warning->setPosition({kPopupW / 2.f, 20.f});
    page->addChild(warning, 2);
}

void VersusMatchPopup::buildBanning(CCNode* page) {
    auto const& match = VersusSession::get().match();
    auto& loc = Localization::get();

    auto* hint = CCLabelBMFont::create(loc.getString("versus.match.ban-hint").c_str(), "goldFont.fnt");
    hint->setScale(0.5f);
    hint->setPosition({kPopupW / 2.f, kPopupH - 62.f});
    page->addChild(hint, 2);

    float const step = 122.f;
    for (size_t i = 0; i < match.offers.size(); i++) {
        auto const& offer = match.offers[i];
        float const x = kPopupW / 2.f + (static_cast<float>(i) - 1.f) * step;

        auto* card = CCNode::create();
        card->setContentSize({110.f, 108.f});
        card->setPosition({x, kPopupH - 132.f});
        page->addChild(card, 2);

        if (auto* panel = paimon::SpriteHelper::safeCreateScale9("square02b_001.png")) {
            panel->setContentSize({110.f, 108.f});
            panel->setPosition({55.f, 54.f});
            panel->setColor({0, 0, 0});
            panel->setOpacity(offer.banned ? 180 : 110);
            card->addChild(panel, 0);
        }

        if (auto* diff = paimon::SpriteHelper::safeCreateWithFrameName(difficultySprite(offer.difficulty))) {
            diff->setScale(0.7f);
            diff->setPosition({55.f, 70.f});
            if (offer.banned) diff->setColor({90, 90, 100});
            card->addChild(diff, 1);
        }

        auto* name = CCLabelBMFont::create(offer.name.c_str(), "bigFont.fnt");
        name->setScale(std::min(0.4f, 96.f / std::max(1.f, name->getContentSize().width)));
        name->setPosition({55.f, 34.f});
        if (offer.banned) name->setColor({110, 110, 120});
        card->addChild(name, 1);

        auto* author = CCLabelBMFont::create(offer.author.c_str(), "chatFont.fnt");
        author->setScale(0.4f);
        author->setOpacity(offer.banned ? 100 : 180);
        author->setPosition({55.f, 18.f});
        card->addChild(author, 1);

        if (offer.banned) {
            auto* stamp = CCLabelBMFont::create(loc.getString("versus.match.banned").c_str(),
                                                "bigFont.fnt");
            stamp->setScale(0.42f);
            stamp->setColor({240, 110, 120});
            stamp->setRotation(-14.f);
            stamp->setPosition({55.f, 54.f});
            card->addChild(stamp, 3);
            continue;
        }

        auto* face = ButtonSprite::create(loc.getString("versus.match.ban").c_str(), 74, true,
                                          "bigFont.fnt", "GJ_button_06.png", 22.f, 0.36f);
        auto* btn = CCMenuItemSpriteExtra::create(face, this, menu_selector(VersusMatchPopup::onBan));
        btn->setTag(kOfferTag + offer.levelId);
        btn->setPosition({x, kPopupH - 200.f});
        m_menu->addChild(btn);
    }

    auto* status = CCLabelBMFont::create(VersusSession::get().statusLine().c_str(), "chatFont.fnt");
    status->setScale(0.46f);
    status->setOpacity(170);
    status->setPosition({kPopupW / 2.f, 30.f});
    page->addChild(status, 2);
}

void VersusMatchPopup::buildLoading(CCNode* page) {
    auto const& match = VersusSession::get().match();
    auto& loc = Localization::get();

    auto* status = CCLabelBMFont::create(VersusSession::get().statusLine().c_str(), "goldFont.fnt");
    status->setScale(0.55f);
    status->setPosition({kPopupW / 2.f, kPopupH - 110.f});
    page->addChild(status, 2);

    if (match.levelId == 0) return;

    auto* playFace = ButtonSprite::create(loc.getString("versus.match.play").c_str(), 140, true,
                                          "bigFont.fnt", "GJ_button_01.png", 32.f, 0.7f);
    auto* play = CCMenuItemSpriteExtra::create(playFace, this, menu_selector(VersusMatchPopup::onPlay));
    play->setPosition({kPopupW / 2.f, kPopupH - 165.f});
    m_menu->addChild(play);
}

void VersusMatchPopup::onAccept(CCObject*) {
    VersusSession::get().accept(true);
}

void VersusMatchPopup::onDecline(CCObject*) {
    VersusSession::get().accept(false);
    Popup::onClose(nullptr);
}

void VersusMatchPopup::onBan(CCObject* sender) {
    VersusSession::get().ban(sender->getTag() - kOfferTag);
}

void VersusMatchPopup::onPlay(CCObject*) {
    // Close first: pushing the scene while the modal is still animating out
    // leaves the fade running over the level.
    Popup::onClose(nullptr);
    VersusSession::get().enterLevel();
}

} // namespace paimon::versus
