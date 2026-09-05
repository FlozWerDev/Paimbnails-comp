#pragma once

// Applies and expires the card effects inside a level.
//
// The rule the whole deck is built on: nothing here touches physics, hitboxes,
// geometry or game speed. Camera, overlays, audio, HUD, checkpoints and shields
// only, so a run with cards is still a legitimate run and the state Globed
// syncs stays clean.

#include "../data/VersusCards.hpp"

#include <Geode/Geode.hpp>

#include <vector>

class PlayLayer;

namespace paimon::versus {

struct ActiveEffect {
    CardId card = CardId::Fog;
    float remaining = 0.f;
    float total = 0.f;
    bool fromRival = false;
};

class VersusEffects {
public:
    static VersusEffects& get();

    void attach(PlayLayer* layer);
    void detach();
    bool attached() const { return m_layer != nullptr; }

    // `fromRival` separates a card the opponent threw at us from one we cast on
    // ourselves; only the first kind can be reflected or dispelled.
    void apply(CardId card, bool fromRival);
    void update(float dt);

    void dispelAll();
    bool has(CardId card) const;
    bool cardsLocked() const;
    bool seesRival() const;
    bool reflectArmed() const { return m_reflect; }
    bool consumeReflect();

    std::vector<ActiveEffect> const& active() const { return m_active; }

private:
    VersusEffects() = default;

    void begin(CardId card, bool fromRival);
    void end(CardId card);
    void endAll();

    cocos2d::CCNode* overlayRoot();
    void addBand(float heightFraction, cocos2d::ccColor4B const& color, char const* id);
    void removeOverlay(char const* id);
    void flash(cocos2d::ccColor4B const& color, float duration);

    void applyCameraTransforms();

    PlayLayer* m_layer = nullptr;
    cocos2d::CCNode* m_overlay = nullptr;

    std::vector<ActiveEffect> m_active;
    bool m_reflect = false;

    float m_baseScale = 1.f;
    bool m_baseScaleValid = false;
    float m_bombTimer = 0.f;
    float m_musicVolume = 1.f;
    float m_effectsVolume = 1.f;
    bool m_audioMuted = false;
};

} // namespace paimon::versus
