#pragma once

#include "PhysicsWorkspace.hpp"

namespace paimon::editorphysics {

// The stand-in for one captured object inside the preview: a real GameObject
// when the game can build one, and a copy of the art it is drawing right now
// when it cannot.
cocos2d::CCNode* buildObjectArt(BodyVisual const& visual);

} // namespace paimon::editorphysics
