// LoadingLayer hook: kicks off the 22 main-level thumbnails and shows their
// progress on Geode's small label. The loading screen never waits for them:
// on a cold cache or a slow drive that would freeze it for seconds, so the
// rest of the preload keeps running in the background behind the menu.

#include <Geode/modify/LoadingLayer.hpp>

#include <fmt/format.h>

#include "../features/backgrounds/services/LayerBackgroundManager.hpp"
#include "../features/icon-gallery/services/GalleryInstaller.hpp"
#include "../framework/HookConventions.hpp"
#include "../features/thumbnails/services/ThumbnailLoader.hpp"
#include "../features/emotes/services/EmoteService.hpp"
#include "../features/emotes/services/EmoteCache.hpp"
#include "../utils/HttpClient.hpp"
#include "../core/MainLevels.hpp"
#include "../core/MainLevelPrefetch.hpp"
#include "../core/PreloadProgress.hpp"
#include "../core/PreloadActions.hpp"
#include "../utils/MainThread.hpp"
#include "../core/RuntimeLifecycle.hpp"
#include <chrono>

using namespace geode::prelude;

namespace {

constexpr float kProgressUpdateInterval = 0.1f;

// Core-set preload lives in core/PreloadActions.cpp
// (paimon::preload::startFullPreload), shared with MenuLayerPreloadFallback.cpp.

} // namespace

class $modify(PaimonLoadingLayer, LoadingLayer) {
    struct Fields {
        bool updateScheduled = false;
        bool setupDone = false;
    };

    static void onModify(auto& self) {
        paimon::hooks::afterNodeIdsOrLate(self, "LoadingLayer::loadAssets");
    }

    bool init(bool fromReload) {
        if (!LoadingLayer::init(fromReload)) {
            return false;
        }
        paimon::captureMainThread();
        // Antes de que More Icons recorra sus pasos de loadAssets: ahi resuelve
        // el icono equipado y borra el guardado si el nuestro no esta en su
        // lista todavia.
        paimon::icon_gallery::GalleryInstaller::registerAllInstalled();
        LayerBackgroundManager::get().applyVanillaBackgroundTintFix(this);
        return true;
    }

    void loadAssets() {
        if (!m_fields->setupDone) {
            m_fields->setupDone = true;

            if (paimon::preload::tryClaimPreload()) {
                paimon::preload::startFullPreload();
            }

            this->updateProgressLabel(0.f);
            this->scheduleProgressUpdates();
        }

        LoadingLayer::loadAssets();
    }

    void scheduleProgressUpdates() {
        if (m_fields->updateScheduled) return;
        m_fields->updateScheduled = true;
        this->schedule(
            schedule_selector(PaimonLoadingLayer::updateProgressLabel),
            kProgressUpdateInterval
        );
    }

    void updateProgressLabel(float /*dt*/) {
        auto label = static_cast<cocos2d::CCLabelBMFont*>(
            this->getChildByID("geode-small-label")
        );
        int loaded = paimon::preload::g_thumbsLoaded.load(std::memory_order_acquire);
        int total = paimon::preload::g_thumbsTotal.load(std::memory_order_acquire);

        if (loaded >= total && total > 0) {
            if (label) label->setString(fmt::format("Paimbnails: miniaturas listas! ({}/{})", loaded, total).c_str());
            this->unschedule(schedule_selector(PaimonLoadingLayer::updateProgressLabel));
            m_fields->updateScheduled = false;
            return;
        }

        if (total == 0) {
            if (label) label->setString("Paimbnails: preparando miniaturas...");
        } else {
            if (label) label->setString(
                fmt::format("Paimbnails: cargando miniaturas... ({}/{})", loaded, total).c_str());
        }
    }

    $override
    void onExit() {
        this->unschedule(schedule_selector(PaimonLoadingLayer::updateProgressLabel));
        m_fields->updateScheduled = false;
        LoadingLayer::onExit();
    }
};
