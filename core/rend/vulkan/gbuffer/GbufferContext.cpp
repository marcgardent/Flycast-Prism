#include "GbufferContext.h"
#include "../../library.h"
#include "../../../oslib/oslib.h"
#include "log/Log.h"
#include <fstream>
#include <iterator>

namespace rend {

GbufferContext::GbufferContext() {
}

void GbufferContext::LoadDefaultConfig() {
    std::string game_id = library::getGameId();
    if (!game_id.empty()) {
        auto filename = hostfs::getHudConfigurationPath();
        LoadConfig(filename + game_id + "/gbuffer_configuration.json");
    }
}

void GbufferContext::LoadConfig(const std::string& filename) {
    std::ifstream i(filename);
    if (!i.is_open()) {
        NOTICE_LOG(RENDERER, "G-Buffer configuration not found: %s", filename.c_str());
        return;
    }

    try {
        std::string content((std::istreambuf_iterator<char>(i)),
                             std::istreambuf_iterator<char>());
        
        auto config = nlohmann::json::parse(content);
        
        m_routingManager.LoadFromJson(config);
        
        // Initializing HudCompositor.
        m_hudCompositor.loadFromJson(content, lastVW, lastVH);
        
        NOTICE_LOG(RENDERER, "Loaded G-Buffer configuration from %s", filename.c_str());
    } catch (const std::exception& e) {
        ERROR_LOG(RENDERER, "Failed to parse %s: %s", filename.c_str(), e.what());
    }
}

void GbufferContext::NewFrame() {
    m_routingManager.NewFrame();
}

void GbufferContext::UpdateViewport(float vW, float vH) {
    NOTICE_LOG(RENDERER, "GbufferContext::UpdateViewport: vW=%.2f, vH=%.2f", vW, vH);
    lastVW = vW; lastVH = vH;
    m_hudCompositor.updateViewport(vW, vH);
    NOTICE_LOG(RENDERER, "HudCompositor transforms count: %zu", m_hudCompositor.getCachedTransforms().size());
}

} // namespace rend
