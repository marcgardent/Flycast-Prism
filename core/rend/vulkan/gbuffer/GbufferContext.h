#pragma once

#include "PolyRoutingManager.h"
#include "HudCompositor.h"
#include <string>

namespace rend {

class GbufferContext {
public:
    GbufferContext();

    /**
     * @brief Loads the default configuration for the current game.
     */
    void LoadDefaultConfig();

    /**
     * @brief Loads the configuration from a specific file.
     * @param filename Path to the configuration JSON.
     */
    void LoadConfig(const std::string& filename);

    /**
     * @brief Resets per-frame state.
     */
    void NewFrame();

    /**
     * @brief Updates viewport dimensions and recalculates HUD positions.
     */
    void UpdateViewport(float vW, float vH);

    PolyRoutingManager& GetRoutingManager() { return m_routingManager; }
    HudCompositor& GetHudCompositor() { return m_hudCompositor; }

private:
    PolyRoutingManager m_routingManager;
    HudCompositor m_hudCompositor;
    float lastVW = 0, lastVH = 0;
};

} // namespace rend
