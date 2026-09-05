#include "VersusHubLayer.hpp"
#include "VersusDeckPopup.hpp"
#include "VersusHistoryPopup.hpp"
#include "VersusLeaderboardLayer.hpp"
#include "VersusMatchPopup.hpp"
#include "VersusSeasonPopup.hpp"
#include "../data/VersusModes.hpp"
#include "../services/VersusClient.hpp"
#include "../services/VersusGlobed.hpp"
#include "../services/VersusSession.hpp"
#include "../services/VersusStore.hpp"
#include "../../backgrounds/services/LayerBackgroundManager.hpp"
#include "../../../utils/Localization.hpp"
#include "../../../utils/PaimonNotification.hpp"
#include "../../../utils/SpriteHelper.hpp"

#include <Geode/ui/BasedButtonSprite.hpp>
#include <Geode/utils/cocos.hpp>

#include <algorithm>

using namespace geode::prelude;
using namespace cocos2d;

namespace paimon::versus {

namespace {

constexpr int kModeTag = 4000;
constexpr int kFormatTag = 4100;

CCLabelBMFont* makeLabel(char const* text, char const* font, float scale, CCPoint const& pos) {
    auto* label = CCLabelBMFont::create(text, font);
    label->setScale(scale);
    label->setPosition(pos);
    return label;
}

} // namespace

VersusHubLayer* VersusHubLayer::create() {
    auto ret = new VersusHubLayer();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

CCScene* VersusHubLayer::scene() {
    auto* scene = CCScene::create();
    scene->addChild(VersusHubLayer::create());
    return scene;
}

bool VersusHubLayer::init() {
    if (!CCLayer::init()) return false;

    m_mode = VersusStore::get().preferredMode();
    this->setKeypadEnabled(true);

    m_menu = CCMenu::create();
    m_menu->setPosition(0.f, 0.f);
    this->addChild(m_menu, 20);

    buildChrome();
    buildRankPanel();
    buildFormatGrid();
    buildActions();
    refreshRank();
    refreshFormats();
    return true;
}

void VersusHubLayer::buildChrome() {
    auto const winSize = CCDirector::get()->getWinSize();

    if (!LayerBackgroundManager::get().applyBackground(this, "versus")) {
        auto* bg = createLayerBG();
        bg->setZOrder(-10);
        this->addChild(bg);
        addSideArt(this, SideArt::All);
    }

    if (auto* logo = paimon::SpriteHelper::safeCreate("paim_vsLogo.png"_spr)) {
        logo->setScale(46.f / std::max(1.f, logo->getContentSize().height));
        logo->setPosition({winSize.width / 2.f, winSize.height - 26.f});
        logo->setColor({255, 226, 140});
        this->addChild(logo, 10);
    }

    auto* backBtn = CCMenuItemSpriteExtra::create(
        CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png"),
        this,
        menu_selector(VersusHubLayer::onBack)
    );
    backBtn->setPosition(25.f, winSize.height - 25.f);
    m_menu->addChild(backBtn);

    m_status = makeLabel("", "chatFont.fnt", 0.5f, {winSize.width / 2.f, 14.f});
    m_status->setOpacity(150);
    this->addChild(m_status, 10);
}

void VersusHubLayer::buildRankPanel() {
    auto const winSize = CCDirector::get()->getWinSize();

    m_rankPanel = CCNode::create();
    m_rankPanel->setPosition({winSize.width * 0.26f, winSize.height * 0.5f});
    this->addChild(m_rankPanel, 5);

    if (auto* panel = paimon::SpriteHelper::safeCreateScale9("square02b_001.png")) {
        panel->setContentSize({168.f, 190.f});
        panel->setPosition({0.f, 0.f});
        panel->setColor({0, 0, 0});
        panel->setOpacity(120);
        m_rankPanel->addChild(panel, -1);
    }

    m_badge = VersusRankBadgeNode::create(VersusStore::get().rank(m_mode), 92.f);
    m_badge->setPosition({0.f, 34.f});
    m_rankPanel->addChild(m_badge, 1);

    m_rankLabel = makeLabel("", "goldFont.fnt", 0.62f, {0.f, -32.f});
    m_rankPanel->addChild(m_rankLabel, 1);

    m_eloLabel = makeLabel("", "bigFont.fnt", 0.44f, {0.f, -52.f});
    m_rankPanel->addChild(m_eloLabel, 1);

    m_recordLabel = makeLabel("", "chatFont.fnt", 0.52f, {0.f, -70.f});
    m_recordLabel->setOpacity(190);
    m_rankPanel->addChild(m_recordLabel, 1);

    // Mode toggle sits above the badge: the two ladders never mix, so switching
    // here swaps everything the panel shows.
    char const* labels[] = {"Classic", "Platformer"};
    for (int i = 0; i < 2; i++) {
        auto* face = ButtonSprite::create(labels[i], 74, true, "bigFont.fnt", "GJ_button_04.png", 24.f, 0.4f);
        auto* btn = CCMenuItemSpriteExtra::create(face, this, menu_selector(VersusHubLayer::onMode));
        btn->setTag(kModeTag + i);
        btn->setPosition(m_rankPanel->getPositionX() + (i == 0 ? -40.f : 40.f),
                         m_rankPanel->getPositionY() + 112.f);
        m_menu->addChild(btn);
        m_modeButtons.push_back(btn);
    }
}

void VersusHubLayer::buildFormatGrid() {
    auto const winSize = CCDirector::get()->getWinSize();

    m_formatPanel = CCNode::create();
    m_formatPanel->setPosition({winSize.width * 0.66f, winSize.height * 0.5f});
    this->addChild(m_formatPanel, 5);

    auto* title = makeLabel(Localization::get().getString("versus.formats").c_str(),
                            "goldFont.fnt", 0.5f, {0.f, 92.f});
    m_formatPanel->addChild(title, 1);
}

void VersusHubLayer::buildActions() {
    auto const winSize = CCDirector::get()->getWinSize();
    auto& loc = Localization::get();

    auto* playFace = ButtonSprite::create(loc.getString("versus.play").c_str(), 150, true,
                                          "bigFont.fnt", "GJ_button_01.png", 30.f, 0.7f);
    auto* playBtn = CCMenuItemSpriteExtra::create(playFace, this, menu_selector(VersusHubLayer::onPlay));
    playBtn->setPosition({winSize.width * 0.66f, winSize.height * 0.5f - 118.f});
    m_menu->addChild(playBtn);

    auto* challengeFace = ButtonSprite::create(loc.getString("versus.challenge").c_str(), 92, true,
                                               "bigFont.fnt", "GJ_button_05.png", 26.f, 0.45f);
    auto* challengeBtn = CCMenuItemSpriteExtra::create(challengeFace, this,
                                                       menu_selector(VersusHubLayer::onChallenge));
    challengeBtn->setPosition({winSize.width * 0.66f - 62.f, winSize.height * 0.5f - 150.f});
    m_menu->addChild(challengeBtn);

    auto* deckFace = ButtonSprite::create(loc.getString("versus.deck").c_str(), 92, true,
                                          "bigFont.fnt", "GJ_button_05.png", 26.f, 0.45f);
    auto* deckBtn = CCMenuItemSpriteExtra::create(deckFace, this, menu_selector(VersusHubLayer::onDeck));
    deckBtn->setPosition({winSize.width * 0.66f + 62.f, winSize.height * 0.5f - 150.f});
    m_menu->addChild(deckBtn);

    if (auto* seasonFace = paimon::SpriteHelper::safeCreateWithFrameName("GJ_sTrendingIcon_001.png")) {
        seasonFace->setScale(0.8f);
        auto* seasonBtn = CCMenuItemSpriteExtra::create(seasonFace, this,
                                                        menu_selector(VersusHubLayer::onSeason));
        seasonBtn->setPosition({winSize.width - 28.f, winSize.height - 98.f});
        m_menu->addChild(seasonBtn);
    }

    if (auto* histFace = paimon::SpriteHelper::safeCreateWithFrameName("GJ_timeIcon_001.png")) {
        histFace->setScale(0.8f);
        auto* histBtn = CCMenuItemSpriteExtra::create(histFace, this,
                                                      menu_selector(VersusHubLayer::onHistory));
        histBtn->setPosition({winSize.width - 28.f, winSize.height - 62.f});
        m_menu->addChild(histBtn);
    }

    auto* boardFace = paimon::SpriteHelper::safeCreateWithFrameName("GJ_rankIcon_001.png");
    if (boardFace) {
        boardFace->setScale(0.85f);
        auto* boardBtn = CCMenuItemSpriteExtra::create(boardFace, this,
                                                       menu_selector(VersusHubLayer::onLeaderboard));
        boardBtn->setPosition({winSize.width - 28.f, winSize.height - 26.f});
        m_menu->addChild(boardBtn);
    }
}

void VersusHubLayer::refreshRank() {
    auto& store = VersusStore::get();
    auto const& profile = store.profile(m_mode);
    auto const rank = store.rank(m_mode);

    m_badge->setRank(rank);
    m_rankLabel->setString(rankName(rank).c_str());
    m_rankLabel->setColor(rankColor(rank));

    if (rank.placing()) {
        m_eloLabel->setString(fmt::format(
            fmt::runtime(Localization::get().getString("versus.placements-left")),
            rank.placementsLeft).c_str());
    } else {
        m_eloLabel->setString(fmt::format("{} Elo", profile.elo).c_str());
    }

    m_recordLabel->setString(fmt::format("{}W - {}L", profile.wins, profile.losses).c_str());

    for (size_t i = 0; i < m_modeButtons.size(); i++) {
        bool const active = static_cast<int>(i) == static_cast<int>(m_mode);
        m_modeButtons[i]->setOpacity(active ? 255 : 140);
        m_modeButtons[i]->setColor(active ? ccColor3B{255, 255, 255} : ccColor3B{150, 150, 150});
    }
}

void VersusHubLayer::refreshFormats() {
    for (auto* btn : m_formatButtons) btn->removeFromParent();
    m_formatButtons.clear();

    auto const formats = rankedFormats(m_mode);
    auto const selected = VersusStore::get().preferredFormat(m_mode);

    int const columns = 4;
    float const step = 58.f;
    float const originY = 52.f;

    for (size_t i = 0; i < formats.size(); i++) {
        auto const* def = formats[i];
        auto* glyph = paimon::SpriteHelper::safeCreate(formatSprite(*def).c_str());
        if (!glyph) continue;

        glyph->setScale(38.f / std::max(1.f, glyph->getContentSize().width));
        glyph->setColor(def->id == selected ? ccColor3B{255, 226, 140} : ccColor3B{190, 196, 214});

        auto* btn = CCMenuItemSpriteExtra::create(glyph, this, menu_selector(VersusHubLayer::onFormat));
        btn->setTag(kFormatTag + static_cast<int>(def->id));

        int const row = static_cast<int>(i) / columns;
        int const col = static_cast<int>(i) % columns;
        int const inRow = std::min<int>(columns, static_cast<int>(formats.size()) - row * columns);
        float const rowWidth = (inRow - 1) * step;

        btn->setPosition({
            m_formatPanel->getPositionX() - rowWidth / 2.f + col * step,
            m_formatPanel->getPositionY() + originY - row * step,
        });
        m_menu->addChild(btn);
        m_formatButtons.push_back(btn);
    }

    auto const& def = formatAt(selected);
    setStatus(formatName(def) + " - " + formatWinCondition(def));
}

void VersusHubLayer::setStatus(std::string const& text, bool error) {
    m_status->setString(text.c_str());
    m_status->setColor(error ? ccColor3B{255, 120, 120} : ccColor3B{255, 255, 255});
}

void VersusHubLayer::onEnterTransitionDidFinish() {
    CCLayer::onEnterTransitionDidFinish();

    auto self = Ref<VersusHubLayer>(this);
    VersusSession::get().addListener(this, [self]() {
        if (self->isRunning()) self->onSessionChanged();
    });
    onSessionChanged();

    if (!gl::present()) {
        setStatus(Localization::get().getString("versus.no-globed"), true);
    } else if (!gl::connected()) {
        setStatus(Localization::get().getString("versus.globed-offline"), true);
    }
}

void VersusHubLayer::onMode(CCObject* sender) {
    auto const mode = static_cast<Mode>(sender->getTag() - kModeTag);
    if (mode == m_mode) return;

    m_mode = mode;
    VersusStore::get().setPreferredMode(mode);
    refreshRank();
    refreshFormats();
}

void VersusHubLayer::onFormat(CCObject* sender) {
    auto const format = static_cast<Format>(sender->getTag() - kFormatTag);
    VersusStore::get().setPreferredFormat(m_mode, format);
    refreshFormats();
}

void VersusHubLayer::authenticateThen(std::function<void()> next) {
    if (VersusClient::get().authenticated()) {
        next();
        return;
    }

    m_busy = true;
    setStatus(Localization::get().getString("versus.connecting"));

    auto self = Ref<VersusHubLayer>(this);
    VersusClient::get().authenticate([self, next = std::move(next)](bool ok, std::string const& message) {
        if (!self->isRunning()) return;
        self->m_busy = false;

        if (!ok) {
            self->setStatus(Localization::get().getString("versus.auth-failed"), true);
            log::warn("[Versus][Hub] Auth failed: {}", message);
            return;
        }
        self->refreshRank();
        next();
    });
}

void VersusHubLayer::onSessionChanged() {
    auto& session = VersusSession::get();

    if (session.phase() == Phase::Found && !m_matchPopupOpen) {
        m_matchPopupOpen = true;
        if (auto* popup = VersusMatchPopup::create()) popup->show();
    }
    if (session.idle()) m_matchPopupOpen = false;

    refreshRank();
    if (session.idle()) return;

    if (session.phase() == Phase::Queued) {
        auto const& ticket = session.ticket();
        setStatus(fmt::format(fmt::runtime(Localization::get().getString("versus.searching")),
                              ticket.waiting, ticket.estimateSeconds));
        return;
    }
    setStatus(session.statusLine());
}

void VersusHubLayer::onPlay(CCObject*) {
    if (m_busy) return;

    // Already queued: this button cancels instead of stacking a second search.
    if (VersusSession::get().phase() == Phase::Queued) {
        VersusSession::get().cancelQueue();
        setStatus(Localization::get().getString("versus.cancelled"));
        return;
    }

    // Ranked needs the fast channel: the Roulette feels like mud without it,
    // and a ladder with two qualities of experience is worse than one rule.
    if (!gl::connected()) {
        setStatus(Localization::get().getString("versus.globed-required"), true);
        return;
    }

    auto self = Ref<VersusHubLayer>(this);
    authenticateThen([self]() {
        VersusSession::get().beginQueue(self->m_mode,
                                        VersusStore::get().preferredFormat(self->m_mode));
    });
}

void VersusHubLayer::onChallenge(CCObject*) {
    PaimonNotify::show(Localization::get().getString("versus.soon-challenge").c_str(),
                       NotificationIcon::Info);
}

void VersusHubLayer::onDeck(CCObject*) {
    if (auto* popup = VersusDeckPopup::create()) popup->show();
}

void VersusHubLayer::onLeaderboard(CCObject*) {
    auto* scene = VersusLeaderboardLayer::scene(m_mode);
    if (!scene) return;
    VersusSession::get().removeListener(this);
    CCDirector::get()->pushScene(CCTransitionFade::create(0.4f, scene));
}

void VersusHubLayer::onHistory(CCObject*) {
    if (auto* popup = VersusHistoryPopup::create()) popup->show();
}

void VersusHubLayer::onSeason(CCObject*) {
    if (auto* popup = VersusSeasonPopup::create()) popup->show();
}

void VersusHubLayer::onBack(CCObject*) {
    keyBackClicked();
}

void VersusHubLayer::keyBackClicked() {
    VersusSession::get().removeListener(this);
    CCDirector::get()->popSceneWithTransition(0.5f, PopTransition::kPopTransitionFade);
}

} // namespace paimon::versus
