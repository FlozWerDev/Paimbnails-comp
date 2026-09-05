#pragma once

#include <cstddef>

namespace paimon::gifimport {

// Rasteriza la decoracion de GD en moldes para el modo libre. Necesita GL y el
// cache de sprites del juego, asi que corre en el hilo principal; el trazado la
// lee ya hecha desde sus hilos. Se construye una vez por sesion.
bool stampLibraryReady();
std::size_t buildStampLibrary();

} // namespace paimon::gifimport
