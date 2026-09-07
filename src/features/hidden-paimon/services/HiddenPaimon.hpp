#pragma once

#include <Geode/Geode.hpp>

// The Paimon that hides in the main menu. She scans the layer for buttons and
// tucks herself under one of them so only her face pokes out, as if watching.
// With the Guide on she stops hiding and waits at the Paimon Hub button, where
// clicking her opens the chat instead of blowing her up.

namespace paimon::hidden_paimon {

inline constexpr char const* kModuleId = "paimbnails.hiddenpaimon.menu";

// Rebuilds her inside layer, dropping any previous one. Safe to call twice.
void attach(cocos2d::CCLayer* layer);

// Re-runs attach on whatever MenuLayer the scene is showing.
void refresh(cocos2d::CCNode* scene);

} // namespace paimon::hidden_paimon
