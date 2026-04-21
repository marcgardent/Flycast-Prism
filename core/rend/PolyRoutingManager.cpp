#include "PolyRoutingManager.h"
#include "hw/pvr/ta_ctx.h"
#include "TexCache.h"
#include "log/LogManager.h"
#include "yaml-cpp/yaml.h"
#include <charconv>
#include <cmath>

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
    LoadConfig("polyrouting.yaml");
}

void PolyRoutingManager::LoadConfig(const std::string& filename) {
    rules.clear();
    try {
        YAML::Node config = YAML::LoadFile(filename);
        if (config["routing"]) {
            for (const auto& node : config["routing"]) {
                Rule rule;
                if (node["match"]) {
                    auto match = node["match"];
                    if (match["x"]) rule.criteria.x = match["x"].as<float>();
                    if (match["y"]) rule.criteria.y = match["y"].as<float>();
                    if (match["z"]) rule.criteria.z = match["z"].as<float>();
                    if (match["count"]) rule.criteria.count = match["count"].as<int>();
                    if (match["texHash"]) {
                        std::string s = match["texHash"].as<std::string>();
                        if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
                            std::from_chars(s.data() + 2, s.data() + s.size(), rule.criteria.texHash, 16);
                        else
                            std::from_chars(s.data(), s.data() + s.size(), rule.criteria.texHash, 16);
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

                if (node["actions"]) {
                    for (const auto& a : node["actions"])
                        rule.actions |= parseAction(a.as<std::string>());
                } else if (node["action"]) {
                    rule.actions |= parseAction(node["action"].as<std::string>());
                }
                rules.push_back(rule);
            }
        }
        NOTICE_LOG(RENDERER, "Loaded %zu polyrouting rules", rules.size());
    } catch (const std::exception& e) {
        // Log error if file exists but parsing failed
        // For now, just ignore if file is missing
    }
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

    if (IsStartHudPass(actions))
        hudPassStarted = true;
    
    if (hudPassStarted)
        actions |= Action_ToHud;

    return actions;
}

} // namespace rend
