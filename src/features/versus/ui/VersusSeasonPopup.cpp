#include "VersusSeasonPopup.hpp"
#include "../data/VersusRanks.hpp"
#include "../services/VersusClient.hpp"
#include "../services/VersusStore.hpp"
#include "../../../utils/DynamicPopupRegistry.hpp"
#include "../../../utils/Localization.hpp"
#include "../../../utils/SpriteHelper.hpp"

#include <algorithm>

using namespace geode::prelude;
using namespace cocos2d;

namespace paimon::versus {

namespace {

constexpr float kPopupW = 340.f;
constexpr float kPopupH = 240.f;

std::string mutatorLabel(std::string const& id) {
    auto const key = "versus.mutator." + id;
    auto text = Localization::get().getString(key);
    return text == key ? id : text;
}

} // namespace

VersusSeasonPopup* VersusSeasonPopup::create() {
    auto ret = new VersusSeasonPopup();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool VersusSeasonPopup::init() {
    if (!Popup::init(kPopupW, kPopupH)) return false;

    paimon::markDynamicPopup(this);

    auto const& season = VersusClient::get().season();
    this->setTitle(season.number > 0
        ? fmt::format("{} {}", Localization::get().getString("versus.season.title"), season.number)
        : Localization::get().getString("versus.season.title"));

    buildSeason();
    buildMutators();
    return true;
}

void VersusSeasonPopup::buildSeason() {
    auto& loc = Localization::get();
    auto const& season = VersusClient::get().season();
    auto& store = VersusStore::get();

    // The best of the two ladders is the one worth showing as the season badge.
    auto const& classic = store.profile(Mode::Classic);
    auto const& platformer = store.profile(Mode::Platformer);
    auto const& best = classic.best >= platformer.best ? classic : platformer;
    auto const rank = rankFor(best.best);

    if (auto* badge = VersusRankBadgeNode::create(rank, 72.f)) {
        badge->setShowPips(false);
        badge->setDim(best.wins + best.losses == 0);
        badge->setPosition({66.f, kPopupH - 108.f});
        m_mainLayer->addChild(badge, 2);
    }

    auto* caption = CCLabelBMFont::create(loc.getString("versus.season.best").c_str(), "chatFont.fnt");
    caption->setScale(0.42f);
    caption->setOpacity(180);
    caption->setPosition({66.f, kPopupH - 154.f});
    m_mainLayer->addChild(caption, 2);

    auto* name = CCLabelBMFont::create(rankName(rank).c_str(), "goldFont.fnt");
    name->setScale(std::min(0.5f, 120.f / std::max(1.f, name->getContentSize().width)));
    name->setColor(rankColor(rank));
    name->setPosition({66.f, kPopupH - 174.f});
    m_mainLayer->addChild(name, 2);

    auto* left = CCLabelBMFont::create(
        season.daysLeft > 0
            ? fmt::format(fmt::runtime(loc.getString("versus.season.ends")), season.daysLeft).c_str()
            : loc.getString("versus.board.loading").c_str(),
        "bigFont.fnt");
    left->setScale(0.5f);
    left->setPosition({kPopupW * 0.66f, kPopupH - 78.f});
    m_mainLayer->addChild(left, 2);

    auto* reset = CCLabelBMFont::create(loc.getString("versus.season.reset").c_str(),
                                        "chatFont.fnt", 180.f, kCCTextAlignmentCenter);
    reset->setScale(0.4f);
    reset->setOpacity(180);
    reset->setPosition({kPopupW * 0.66f, kPopupH - 122.f});
    m_mainLayer->addChild(reset, 2);
}

void VersusSeasonPopup::buildMutators() {
    auto& loc = Localization::get();
    auto const& mutators = VersusClient::get().season().mutators;

    auto* heading = CCLabelBMFont::create(loc.getString("versus.mutators").c_str(), "goldFont.fnt");
    heading->setScale(0.44f);
    heading->setPosition({kPopupW / 2.f, 62.f});
    m_mainLayer->addChild(heading, 2);

    if (mutators.empty()) {
        auto* none = CCLabelBMFont::create(loc.getString("versus.mutator.none").c_str(), "chatFont.fnt");
        none->setScale(0.44f);
        none->setOpacity(170);
        none->setPosition({kPopupW / 2.f, 40.f});
        m_mainLayer->addChild(none, 2);
        return;
    }

    for (size_t i = 0; i < mutators.size(); i++) {
        auto* label = CCLabelBMFont::create(mutatorLabel(mutators[i]).c_str(), "chatFont.fnt",
                                            290.f, kCCTextAlignmentCenter);
        label->setScale(0.44f);
        label->setColor({255, 200, 130});
        label->setPosition({kPopupW / 2.f, 40.f - static_cast<float>(i) * 18.f});
        m_mainLayer->addChild(label, 2);
    }
}

} // namespace paimon::versus
