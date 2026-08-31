#pragma once

#include "../GifImportTypes.hpp"

namespace paimon::gifimport {

// Duplica las figuras de los colores que brillan un poco mas grandes y detras,
// con su propio canal mezclado y a media opacidad. Es el mismo truco con el que
// se hace el glow a mano en el editor, asi que no depende de que exista un
// objeto de glow concreto en la version de GD que tenga el jugador.
void applyGlow(ImportPlan& plan, GlowMode mode, std::size_t objectBudget);

} // namespace paimon::gifimport
