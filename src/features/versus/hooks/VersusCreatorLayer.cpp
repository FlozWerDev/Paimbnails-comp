#include <Geode/Geode.hpp>
#include <Geode/modify/CreatorLayer.hpp>

#include "../ui/VersusHubLayer.hpp"
#include "../services/VersusStore.hpp"
#include "../../../core/modules/ModuleRegistry.hpp"
#include "../../../framework/HookConventions.hpp"
#include "../../../utils/SpriteHelper.hpp"
#include "../../progression/ui/TierBadgeNode.hpp"
#include "../../transitions/services/TransitionManager.hpp"

#include <algorithm>

using namespace geode::prelude;
using namespace paimon::versus;

namespace {

constexpr char const* kModuleId = "paimbnails.versus.menu";
constexpr char const* kFaceId = "versus-face"_spr;
constexpr char const* kChipId = "versus-chip"_spr;

void fitSquare(CCNode* node, float size) {
    float const source = std::max(node->getContentSize().width, node->getContentSize().height);
    node->setScale(size / std::max(1.f, source));
}

// The rank chip in the corner: the tier plate the player currently sits at, so
// the ladder is readable from the menu without opening anything.
CCNode* buildRankChip(float size) {
    auto* chip = CCNode::create();
    chip->setContentSize({size, size});
    chip->setAnchorPoint({0.5f, 0.5f});

    auto const rank = VersusStore::get().rank(VersusStore::get().preferredMode());

    if (auto* plate = paimon::SpriteHelper::safeCreate("paim_vsChip.png"_spr)) {
        fitSquare(plate, size);
        plate->setColor(rank.placing() ? ccColor3B{120, 126, 142}
                                       : paimon::versus::rankColor(rank));
        plate->setPosition({size / 2.f, size / 2.f});
        chip->addChild(plate, 0);
    }

    if (auto* medal = paimon::progression::makeTierPlate(
            paimon::progression::tierAt(rank.tierIndex).frame)) {
        fitSquare(medal, size * 0.62f);
        medal->setColor(paimon::progression::tierAt(rank.tierIndex).accent);
        medal->setPosition({size / 2.f, size / 2.f});
        chip->addChild(medal, 1);
    }

    return chip;
}

} // namespace

class $modify(PaimonVersusCreatorLayer, CreatorLayer) {
    static void onModify(auto& self) {
        paimon::hooks::afterNodeIdsOrLate(self, "CreatorLayer::init");
    }

    $override
    bool init() {
        if (!CreatorLayer::init()) return false;
        if (!paimon::modules::isEnabled(kModuleId)) return true;

        auto* menu = this->getChildByID("creator-buttons-menu");
        if (!menu) return true;

        auto* versus = typeinfo_cast<CCMenuItemSpriteExtra*>(menu->getChildByID("versus-button"));
        if (versus) {
            this->reskinVersusButton(versus);
            return true;
        }

        // Some other mod took the button away, or a future GD moved it. Add our
        // own so the entry point never disappears.
        auto* face = paimon::SpriteHelper::safeCreate("paim_vsBtn.png"_spr);
        if (!face) return true;
        fitSquare(face, 60.f);
        face->setColor({232, 96, 112});

        auto* btn = CCMenuItemSpriteExtra::create(
            face, this, menu_selector(PaimonVersusCreatorLayer::onPaimonVersus));
        btn->setID("paimon-versus-button"_spr);
        menu->addChild(btn);
        menu->updateLayout();
        return true;
    }

    void reskinVersusButton(CCMenuItemSpriteExtra* button) {
        // Retargeting the node GD already laid out keeps its size, its hitbox
        // and its place in the row, so nothing has to be re-measured.
        button->setTarget(this, menu_selector(PaimonVersusCreatorLayer::onPaimonVersus));

        if (button->getChildByID(kFaceId)) return;

        auto const size = button->getContentSize();
        if (auto* original = button->getNormalImage()) original->setVisible(false);

        if (auto* face = paimon::SpriteHelper::safeCreate("paim_vsBtn.png"_spr)) {
            fitSquare(face, std::max(size.width, size.height));
            face->setColor({232, 96, 112});
            face->setPosition({size.width / 2.f, size.height / 2.f});
            face->setID(kFaceId);
            button->addChild(face, 1);
        }

        if (auto* chip = buildRankChip(size.width * 0.42f)) {
            chip->setPosition({size.width * 0.80f, size.height * 0.20f});
            chip->setID(kChipId);
            button->addChild(chip, 2);
        }
    }

    void onPaimonVersus(CCObject*) {
        auto* scene = VersusHubLayer::scene();
        if (!scene) return;
        TransitionManager::get().pushScene(scene);
    }
};
