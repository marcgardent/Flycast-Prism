#pragma once

#include <vector>
#include <string>
#include <map>
#include "json.hpp"

struct Rect {
    float x, y, w, h;
    bool intersects(const Rect& o) const;
};

struct Vector2 { float x, y; };

enum class Anchor {
    SCREEN_TOP_LEFT, SCREEN_TOP_MID, SCREEN_TOP_RIGHT,
    SCREEN_LEFT_MID, SCREEN_CENTER, SCREEN_RIGHT_MID,
    SCREEN_BOTTOM_LEFT, SCREEN_BOTTOM_MID, SCREEN_BOTTOM_RIGHT,
    SAFE_ZONE_TOP_LEFT, SAFE_ZONE_TOP_MID, SAFE_ZONE_TOP_RIGHT,
    SAFE_ZONE_LEFT_MID, SAFE_ZONE_RIGHT_MID,
    SAFE_ZONE_BOTTOM_LEFT, SAFE_ZONE_BOTTOM_MID, SAFE_ZONE_BOTTOM_RIGHT,
    UNKNOWN
};

struct HudZoneDefinition {
    std::string name;
    Rect mappingRect;
    Anchor mappingAnchor;
    float scale;
    bool zenMode;
};

/**
 * @brief Transformed HUD element ready for rendering
 */
struct TransformedHudElement {
    std::string name;
    Rect viewportRect;
    Rect sourceRect;
    bool zenMode;
};


class HudCompositor {

    float screenW = 0, screenH = 0;
    const float VIRT_W = 640.0f, VIRT_H = 480.0f;

    std::vector<HudZoneDefinition> m_definitions;
    std::vector<TransformedHudElement> m_cachedTransforms;
    std::map<Anchor, Vector2> m_anchorTable;

    void refreshAnchorTable();
    void recalculate();
    static Anchor strToAnchor(const std::string& s);

public:
    /**
     * @brief Loads the HUD configuration, validates the safe zone, and calculates initial positions.
     */
    bool loadFromJson(const std::string& jsonStr, float vW, float vH);

    /**
     * @brief Updates viewport dimensions and recalculates rectangles (with pruning).
     */
    void updateViewport(float vW, float vH);

    /**
     * @brief Returns the pre-calculated list of HUD elements (only those without overlaps).
     */
    const std::vector<TransformedHudElement>& getTransforms() const {
        return m_cachedTransforms;
    }
};