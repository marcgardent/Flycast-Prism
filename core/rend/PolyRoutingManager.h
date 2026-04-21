#pragma once
#include "types.h"
#include <vector>
#include <string>

namespace rend {

enum PolyAction : u32 {
    Action_None          = 0,
    Action_ToHud         = 1 << 0,
    Action_StartHudPass  = 1 << 1,
    Action_AvoidAlbedo   = 1 << 2,
    Action_AvoidNormal   = 1 << 3,
    Action_AvoidMaterial = 1 << 4,
    Action_AvoidMotion   = 1 << 5,
    Action_AvoidDepth    = 1 << 6,
};

class PolyRoutingManager {
public:
    struct Criteria {
        float x = -1.0f, y = -1.0f, z = -1.0f;
        u32 texHash = 0;
        int count = -1;

        bool matches(float vx, float vy, float vz, int pCount, u32 pTexHash) const;
    };

    struct Rule {
        Criteria criteria;
        u32 actions = Action_None;
    };

    struct PolyMatchParams {
        float x, y, z;
        int count;
        u32 texHash;
    };

    PolyRoutingManager();
    void LoadConfig(const std::string& filename);
    void NewFrame();

    u32 GetActions(const PolyMatchParams& params);

    static bool IsToHud(u32 actions) { return (actions & Action_ToHud) != 0; }
    static bool IsStartHudPass(u32 actions) { return (actions & Action_StartHudPass) != 0; }
    static bool IsAvoidAlbedo(u32 actions) { return (actions & Action_AvoidAlbedo) != 0; }
    static bool IsAvoidNormal(u32 actions) { return (actions & Action_AvoidNormal) != 0; }
    static bool IsAvoidMaterial(u32 actions) { return (actions & Action_AvoidMaterial) != 0; }
    static bool IsAvoidMotion(u32 actions) { return (actions & Action_AvoidMotion) != 0; }
    static bool IsAvoidDepth(u32 actions) { return (actions & Action_AvoidDepth) != 0; }

private:
    std::vector<Rule> rules;
    bool hudPassStarted = false;
};

} // namespace rend
