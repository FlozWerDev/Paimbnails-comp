#include "VersusInit.hpp"
#include "services/VersusNet.hpp"
#include "services/VersusStore.hpp"
#include "../../core/modules/ModuleRegistry.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace paimon::versus {

void init() {
    if (!paimon::modules::isEnabled("paimbnails.versus.menu")) {
        log::info("[Versus][Init] Module off, skipping");
        return;
    }

    VersusStore::get().load();
    net::registerEvents();

    auto const rank = VersusStore::get().rank(VersusStore::get().preferredMode());
    log::info("[Versus][Init] Ready at {} ({} Elo)", rankName(rank), rank.elo);
}

} // namespace paimon::versus
