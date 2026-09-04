#pragma once

#include "../PhysicsNative.hpp"

#include <vector>

namespace paimon::editorphysics {

// A reactive body never runs the solver inside GD: it runs the Advanced Follow
// graph the emitter writes. Showing it in the lab with the rigid trajectory is
// what made the preview and the level disagree, so every body is traced on the
// backend it will actually compile to, and only the baked ones keep the solver.
SimulationTrace simulateWorkspace(
    std::vector<BodySpec> const& bodies,
    std::vector<NativeBodySettings> const& settings,
    SimulationOptions const& options
);

// True when the graph cannot move the body without the player being there, so
// the lab can say why it is standing still instead of falling.
bool nativeNeedsPlayer(NativeBodySettings const& settings, BodySpec const& body, float gravity);

} // namespace paimon::editorphysics
