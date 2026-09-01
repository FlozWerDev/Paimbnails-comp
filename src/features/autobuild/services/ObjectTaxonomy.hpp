#pragma once

// What an object *is*, as far as a save string can tell.
//
// GD ships thousands of object ids and the game itself is the only place that
// knows all of them, so this is deliberately two-sided: a curated table for the
// ids whose behaviour matters (hazards, portals, pads, orbs, triggers) and a
// structural read of the save string for everything else. The curated half can
// be corrected without a rebuild through config/autobuild/objects.txt.

#include <string>
#include <vector>

#include "LevelParse.hpp"

namespace paimon::autobuild {

enum class ObjectKind : unsigned char {
    Unknown,
    Solid,
    Slope,
    Hazard,
    Portal,
    Pad,
    Orb,
    Collectible,
    Trigger,
    Deco,
    Text,
    Particle,
};

char const* kindName(ObjectKind kind);

ObjectKind kindOf(int objectId);

// "1338 solid" / "8 hazard" lines, one per object id. Unknown names are
// ignored so a file written for a newer build still loads.
int loadTaxonomyOverrides(std::string const& text);
void clearTaxonomyOverrides();

// Trigger detection that does not depend on the id table: only triggers carry
// the activation and target keys GD writes for them.
bool looksLikeTrigger(LevelObject const& object);

// True when the object is one of the autobuild marker blocks.
bool isMarkerObject(int objectId);

} // namespace paimon::autobuild
