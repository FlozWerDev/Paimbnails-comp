#pragma once

#include <atomic>

namespace paimon::preload {

inline std::atomic<int> g_thumbsTotal{0};
inline std::atomic<int> g_thumbsLoaded{0};

inline std::atomic<int> g_emotesTotal{0};
inline std::atomic<int> g_emotesLoaded{0};

inline std::atomic<bool> g_emotesCatalogReady{false};

inline std::atomic<bool> g_preloadStarted{false};

// Set from $on_game(Loaded). Deferred preload work waits on it so it never
// competes with the game's own asset loading on slow machines.
inline std::atomic<bool> g_gameLoaded{false};

inline int getTotalLoaded() {
    return g_thumbsLoaded.load(std::memory_order_relaxed)
         + g_emotesLoaded.load(std::memory_order_relaxed);
}

inline int getTotalCount() {
    return g_thumbsTotal.load(std::memory_order_relaxed)
         + g_emotesTotal.load(std::memory_order_relaxed);
}

inline bool isFinished() {
    int total = getTotalCount();
    return total > 0 && getTotalLoaded() >= total;
}

inline bool isWaitingForEmoteCatalog() {
    return !g_emotesCatalogReady.load(std::memory_order_acquire);
}

inline bool tryClaimPreload() {
    bool expected = false;
    return g_preloadStarted.compare_exchange_strong(expected, true);
}

inline void resetPreloadState() {
    g_thumbsLoaded.store(0, std::memory_order_relaxed);
    g_thumbsTotal.store(0, std::memory_order_relaxed);
    g_emotesLoaded.store(0, std::memory_order_relaxed);
    g_emotesTotal.store(0, std::memory_order_relaxed);
    g_emotesCatalogReady.store(false, std::memory_order_release);
    g_preloadStarted.store(false, std::memory_order_release);
}

} // namespace paimon::preload
