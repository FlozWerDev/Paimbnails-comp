#pragma once

// Estado compartido de Paimon RTX: carga/guarda la config y decide si el
// postproceso debe correr en la escena actual. El renderer lo consulta una vez
// por fotograma desde swapBuffers.

#include "RTXConfig.hpp"

#include <filesystem>

namespace paimon::rtx {

class RTXManager {
public:
    static RTXManager& get();

    RTXConfig& config() { return m_config; }
    RTXConfig const& config() const { return m_config; }

    void init();
    void loadConfig();
    void saveConfig();
    void resetToDefaults();

    // Interruptor propio, sin mirar el modulo ni la escena.
    bool isEnabled() const;
    void setEnabled(bool enabled);

    // Lo que consulta el renderer: modulo + interruptor + ambito de la escena.
    bool shouldRender() const;

private:
    RTXManager() = default;
    RTXManager(RTXManager const&) = delete;
    RTXManager& operator=(RTXManager const&) = delete;

    std::filesystem::path configPath() const;
    void sanitize();

    RTXConfig m_config;
    bool m_loaded = false;
};

} // namespace paimon::rtx
