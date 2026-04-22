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

bool PolyRoutingManager::Criteria::matches(float vx, float vy, float vz, int pCount, u32 pTexHash) const {
    if (x != -1.0f && std::abs(vx - x) > 0.001f) return false;
    if (y != -1.0f && std::abs(vy - y) > 0.001f) return false;
    if (z != -1.0f && std::abs(vz - z) > 0.001f) return false;
    if (count != -1 && pCount != count) return false;
    if (texHash != 0 && pTexHash != texHash) return false;
    return true;
}

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
    rules.clear();
    if (config.contains("routing") && config["routing"].is_array()) {
        for (const auto& node : config["routing"]) {
            Rule rule;
            if (node.contains("match")) {
                auto& match = node["match"];
                if (match.contains("x")) rule.criteria.x = match["x"].get<float>();
                if (match.contains("y")) rule.criteria.y = match["y"].get<float>();
                if (match.contains("z")) rule.criteria.z = match["z"].get<float>();
                if (match.contains("count")) rule.criteria.count = match["count"].get<int>();
                if (match.contains("texHash")) {
                    if (match["texHash"].is_string()) {
                        std::string s = match["texHash"].get<std::string>();
                        if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
                            std::from_chars(s.data() + 2, s.data() + s.size(), rule.criteria.texHash, 16);
                        else
                            std::from_chars(s.data(), s.data() + s.size(), rule.criteria.texHash, 16);
                    } else {
                        rule.criteria.texHash = match["texHash"].get<u32>();
                    }
                }
            }

            auto parseAction = [](const std::string& s) -> u32 {
                if (s == "toHud") return Action_ToHud;
                if (s == "startHudPass") return Action_StartHudPass;
                if (s == "avoidAlbedo") return Action_AvoidAlbedo;
                if (s == "avoidNormal") return Action_AvoidNormal;
                if (s == "avoidMaterial") return Action_AvoidMaterial;
                if (s == "avoidMotion") return Action_AvoidMotion;
                if (s == "avoidDepth") return Action_AvoidDepth;
                return Action_None;
            };

            if (node.contains("actions") && node["actions"].is_array()) {
                for (const auto& a : node["actions"])
                    rule.actions |= parseAction(a.get<std::string>());
            } else if (node.contains("action")) {
                rule.actions |= parseAction(node["action"].get<std::string>());
            }
            rules.push_back(rule);
        }
    }
    NOTICE_LOG(RENDERER, "Loaded %zu polyrouting rules", rules.size());
}

void PolyRoutingManager::NewFrame() {
    hudPassStarted = false;
}

u32 PolyRoutingManager::GetActions(const PolyMatchParams& params) {
    u32 actions = Action_None;
    
    for (const auto& rule : rules) {
        if (rule.criteria.matches(params.x, params.y, params.z, params.count, params.texHash)) {
            actions |= rule.actions;
        }
    }

    if (actions & rend::Action_StartHudPass)
        hudPassStarted = true;
    
    if (hudPassStarted)
        actions |= Action_ToHud;

    return actions;
}

} // namespace rend
