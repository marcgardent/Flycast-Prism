#include "PolyRoutingManager.h"
#include "hw/pvr/ta_ctx.h"
#include "../../TexCache.h"
#include "log/LogManager.h"
#include "json.hpp"
#include <fstream>
#include <charconv>
#include <cmath>

#include "../../library.h"

namespace rend {

PolyRoutingManager::PolyRoutingManager() {
}

void PolyRoutingManager::LoadConfig(const std::string& filename) {
    std::ifstream i(filename);
    if (!i.is_open()) {
        NOTICE_LOG(RENDERER, "poly_routing configuration not found: %s", filename.c_str());
        return;
    }

    try {
        nlohmann::json config;
        i >> config;
        LoadFromJson(config);
    } catch (const std::exception& e) {
        ERROR_LOG(RENDERER, "Failed to parse %s: %s", filename.c_str(), e.what());
    }
}

void PolyRoutingManager::LoadFromJson(const nlohmann::json& config) {
    hudEvaluator.reset();
    skyEvaluator.reset();
    sceneEvaluator.reset();

    if (config.contains("stencils") && config["stencils"].is_object()) {
        auto& stencils = config["stencils"];
        
        auto loadStencil = [](const nlohmann::json& j, const std::string& key) -> std::unique_ptr<PolyRequestEvaluator> {
            if (j.contains(key) && j[key].is_string()) {
                auto evaluator = std::make_unique<PolyRequestEvaluator>();
                if (evaluator->parse(j[key].get<std::string>())) {
                    return evaluator;
                }
            }
            return nullptr;
        };

        hudEvaluator = loadStencil(stencils, "HUD");
        skyEvaluator = loadStencil(stencils, "SKY");
        sceneEvaluator = loadStencil(stencils, "SCENE");
    }
    
    NOTICE_LOG(RENDERER, "PolyRoutingManager: Loaded stencils (HUD: %d, SKY: %d, SCENE: %d)", 
               hudEvaluator != nullptr, skyEvaluator != nullptr, sceneEvaluator != nullptr);
}

void PolyRoutingManager::NewFrame() {
}

bool PolyRoutingManager::IsHud(const PolyData& data) const {
    return hudEvaluator && hudEvaluator->evaluate(data) > 0.5;
}

bool PolyRoutingManager::IsSky(const PolyData& data) const {
    return skyEvaluator && skyEvaluator->evaluate(data) > 0.5;
}

bool PolyRoutingManager::IsScene(const PolyData& data) const {
    return sceneEvaluator && sceneEvaluator->evaluate(data) > 0.5;
}

} // namespace rend
