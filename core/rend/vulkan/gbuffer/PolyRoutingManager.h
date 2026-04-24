#pragma once
#include "types.h"
#include <vector>
#include <string>
#include <memory>
#include "json.hpp"
#include "PolyRequestEvaluator.h"

namespace rend {

class PolyRoutingManager {
public:
    PolyRoutingManager();

    void LoadFromJson(const nlohmann::json& config);
    void LoadConfig(const std::string& filename);
    void NewFrame();

    bool IsHud(const PolyData& data) const;
    bool IsSky(const PolyData& data) const;
    bool IsScene(const PolyData& data) const;

private:
    std::unique_ptr<PolyRequestEvaluator> hudEvaluator;
    std::unique_ptr<PolyRequestEvaluator> skyEvaluator;
    std::unique_ptr<PolyRequestEvaluator> sceneEvaluator;
};

} // namespace rend
