// LoadingLayer hook: preloads the 22 main-level thumbnails before the menu.
// The wait is capped and only armed when they are already on disk, so a cold
// cache or a slow connection never holds the game on the loading screen.

#include <Geode/modify/LoadingLayer.hpp>

#include <fmt/format.h>

#include "../features/backgrounds/services/LayerBackgroundManager.hpp"
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
constexpr int kFinalGameLoadStep = 14;
// Hard cap on how long the game may sit on the last load step waiting for us.
// Past it the preload keeps running and MenuLayer shows the rest of it.
constexpr auto kMainLevelGateTimeout = std::chrono::seconds(2);

// Core-set preload lives in core/PreloadActions.cpp
// (paimon::preload::startFullPreload), shared with MenuLayerPreloadFallback.cpp.

} // namespace

class $modify(PaimonLoadingLayer, LoadingLayer) {
    struct Fields {
        bool updateScheduled = false;
        bool setupDone = false;
        bool waitingForMainLevels = false;
        bool mainLevelsFinished = false;
        bool gateArmed = false;
        std::chrono::steady_clock::time_point gateDeadline{};
    };

    static void onModify(auto& self) {
        paimon::hooks::afterNodeIdsOrLate(self, "LoadingLayer::loadAssets");
    }

    bool init(bool fromReload) {
        if (!LoadingLayer::init(fromReload)) {
            return false;
        }
        paimon::captureMainThread();
        LayerBackgroundManager::get().applyVanillaBackgroundTintFix(this);
        return true;
    }

    void loadAssets() {
        if (!m_fields->setupDone) {
            m_fields->setupDone = true;

            if (paimon::preload::tryClaimPreload()) {
                paimon::preload::startFullPreload();
            }

            // Waiting only pays off when every thumbnail is already on disk.
            // A cold cache means downloads, and those take minutes on a slow
            // connection with the loading screen frozen behind them.
            m_fields->gateArmed = paimon::areMainLevelsFreshlyCached();
            m_fields->gateDeadline = std::chrono::steady_clock::now() + kMainLevelGateTimeout;
            if (!m_fields->gateArmed) {
                log::info("[Paimbnails Preload] Main levels not on disk yet; loading screen will not wait for them");
            }

            this->updateProgressLabel(0.f);
            this->scheduleProgressUpdates();
        }

        int loaded = paimon::preload::g_thumbsLoaded.load(std::memory_order_acquire);
        int total = paimon::preload::g_thumbsTotal.load(std::memory_order_acquire);
        if (m_loadStep == kFinalGameLoadStep && !m_fields->mainLevelsFinished) {
            if (m_fields->gateArmed && total > 0 && loaded < total && !this->gateExpired()) {
                m_fields->waitingForMainLevels = true;
                return;
            }
            m_fields->mainLevelsFinished = true;
        }

        LoadingLayer::loadAssets();
    }

    bool gateExpired() {
        return std::chrono::steady_clock::now() >= m_fields->gateDeadline;
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

        bool const done = total > 0 && loaded >= total;
        bool const expired = this->gateExpired();

        std::string text;
        if (done) {
            text = fmt::format("Paimbnails: miniaturas listas! ({}/{})", loaded, total);
        } else if (expired) {
            text = "Paimbnails: miniaturas cargando en segundo plano...";
        } else if (total == 0) {
            text = "Paimbnails: preparando miniaturas...";
        } else {
            text = fmt::format("Paimbnails: cargando miniaturas... ({}/{})", loaded, total);
        }
        if (label) label->setString(text.c_str());

        // Give the label back to Geode once we stop gating, so the rest of the
        // screen shows what the game is really loading instead of our text.
        if (done || expired) {
            this->unschedule(schedule_selector(PaimonLoadingLayer::updateProgressLabel));
            m_fields->updateScheduled = false;
        }

        if (m_fields->waitingForMainLevels && (done || expired)) {
            m_fields->waitingForMainLevels = false;
            m_fields->mainLevelsFinished = true;
            LoadingLayer::loadAssets();
        }
    }

    $override
    void onExit() {
        this->unschedule(schedule_selector(PaimonLoadingLayer::updateProgressLabel));
        m_fields->updateScheduled = false;
        LoadingLayer::onExit();
    }
};
