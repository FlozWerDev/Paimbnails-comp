#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace cocos2d { class CCTexture2D; }

namespace paimon {

struct ThumbnailLoadedEvent {
    int levelID = 0;
    std::string source;   // "network", "disk-cache", "ram-cache"
    bool isGif = false;
};

struct CacheEvictedEvent {
    std::string key;
    std::string reason;   // "lru", "ttl", "manual"
    size_t freedBytes = 0;
};

struct UploadCompletedEvent {
    int levelID = 0;
    std::string format;   // "png", "gif", "mp4"
    std::string username;
    bool success = false;
    std::string message;
};

struct FeatureToggledEvent {
    std::string featureName;
    bool enabled = false;
};

struct PermissionDeniedEvent {
    std::string featureName;
    std::string action;
    std::string reason;
};

struct UploadStartedEvent {
    int levelID = 0;
    std::string format;
    std::string username;
    size_t dataSize = 0;
};

struct AudioOwnerChangedEvent {
    std::string previous;  // "none", "menu", "dynamic", "profile", "preview"
    std::string current;
    int sessionToken = 0;
};

// Cambia el fondo y avisa a los suscritos.
struct ThumbnailBackgroundChangedEvent {
    int levelID = 0;
    geode::Ref<cocos2d::CCTexture2D> texture = nullptr;

// Lo lee InfoLayer al abrir, sin esperar al siguiente ciclo.
// Puntero crudo con retain manual: un estatico con destructor reventaria en atexit.
    static inline int s_lastLevelID = 0;
    static inline cocos2d::CCTexture2D* s_lastTextureRaw = nullptr;

    // Solo hilo principal.
    static void setLastTexture(cocos2d::CCTexture2D* tex) {
        if (s_lastTextureRaw == tex) return;
        if (tex) tex->retain();
        if (s_lastTextureRaw) s_lastTextureRaw->release();
        s_lastTextureRaw = tex;
    }

    static cocos2d::CCTexture2D* getLastTexture() {
        return s_lastTextureRaw;
    }
};

} // namespace paimon
