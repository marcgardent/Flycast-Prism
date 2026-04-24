#pragma once
#include <string>
#include <vector>
#include <map>

constexpr float VIRT_W = 640.0f;
constexpr float VIRT_H = 480.0f;

struct Vector2 {
    float x, y;
};

struct Rect {
    float x, y, w, h;
    bool intersects(const Rect& o) const;
};

enum class Anchor {
    UNKNOWN,
    SCREEN_TOP_LEFT, SCREEN_TOP_MID, SCREEN_TOP_RIGHT,
    SCREEN_LEFT_MID, SCREEN_CENTER, SCREEN_RIGHT_MID,
    SCREEN_BOTTOM_LEFT, SCREEN_BOTTOM_MID, SCREEN_BOTTOM_RIGHT,
    SAFE_ZONE_TOP_LEFT, SAFE_ZONE_TOP_MID, SAFE_ZONE_TOP_RIGHT,
    SAFE_ZONE_LEFT_MID, SAFE_ZONE_RIGHT_MID,
    SAFE_ZONE_BOTTOM_LEFT, SAFE_ZONE_BOTTOM_MID, SAFE_ZONE_BOTTOM_RIGHT
};

struct HudZoneDef {
    std::string name;

    Rect sourceRect;
    Anchor sourceAnchor;

    Rect destRect;
    Anchor mappingAnchor;

    bool zenMode;
};

struct ViewportTransform {
    std::string name;
    Rect source;
    Rect destination;
    bool zenMode;
};

class HudCompositor {
public:
    bool loadFromJson(const std::string& jsonStr, float vW, float vH);
    void updateViewport(float vW, float vH);

    const std::vector<ViewportTransform>& getCachedTransforms() const { return m_cachedTransforms; }

private:
    void refreshAnchorTable();
    void recalculate();
    static Anchor strToAnchor(const std::string& s);

    float screenW = 0.0f;
    float screenH = 0.0f;

    std::map<Anchor, Vector2> m_anchorTable;
    std::vector<HudZoneDef> m_definitions;
    std::vector<ViewportTransform> m_cachedTransforms;
};