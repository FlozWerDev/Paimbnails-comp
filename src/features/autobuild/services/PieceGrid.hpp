#pragma once

// Turning loose objects into wave pieces. Split out of Capture so the level
// analyzer and the regression test can build templates without the editor.

#include <string>
#include <vector>

#include "../AutobuildTypes.hpp"

namespace paimon::autobuild {

// Two pieces with the same signature are the same shape with the same object
// state, so they collapse into one tile with a higher weight.
std::string pieceSignature(Piece const& piece);

// Move a piece's objects so their bounding box is centred on the origin.
void centerPiece(Piece& piece);

// Wave template out of loose objects whose dx/dy hold absolute level positions.
Template waveFromObjects(std::vector<CapturedObject> objects, float cell);

} // namespace paimon::autobuild
