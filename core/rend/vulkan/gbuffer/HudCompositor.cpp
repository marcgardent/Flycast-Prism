#include "HudCompositor.h"
#include <cmath>
#include "log/Log.h"

bool Rect::intersects(const Rect& o) const {
    return (x < o.x + o.w && x + w > o.x && y < o.y + o.h && y + h > o.y);
}

void HudCompositor::refreshAnchorTable() {
    float scale = screenH / VIRT_H;
    float safeW = VIRT_W * scale;
    float safeX = (screenW - safeW) / 2.0f;

    m_anchorTable = {
        {Anchor::SCREEN_TOP_LEFT,     {0, 0}},
        {Anchor::SCREEN_TOP_MID,      {screenW / 2.0f, 0}},
        {Anchor::SCREEN_TOP_RIGHT,    {screenW, 0}},
        {Anchor::SCREEN_LEFT_MID,     {0, screenH / 2.0f}},
        {Anchor::SCREEN_CENTER,       {screenW / 2.0f, screenH / 2.0f}},
        {Anchor::SCREEN_RIGHT_MID,    {screenW, screenH / 2.0f}},
        {Anchor::SCREEN_BOTTOM_LEFT,  {0, screenH}},
        {Anchor::SCREEN_BOTTOM_MID,   {screenW / 2.0f, screenH}},
        {Anchor::SCREEN_BOTTOM_RIGHT, {screenW, screenH}},
        {Anchor::SAFE_ZONE_TOP_LEFT,     {safeX, 0}},
        {Anchor::SAFE_ZONE_TOP_MID,      {screenW / 2.0f, 0}},
        {Anchor::SAFE_ZONE_TOP_RIGHT,    {safeX + safeW, 0}},
        {Anchor::SAFE_ZONE_LEFT_MID,     {safeX, screenH / 2.0f}},
        {Anchor::SAFE_ZONE_RIGHT_MID,    {safeX + safeW, screenH / 2.0f}},
        {Anchor::SAFE_ZONE_BOTTOM_LEFT,  {safeX, screenH}},
        {Anchor::SAFE_ZONE_BOTTOM_MID,   {screenW / 2.0f, screenH}},
        {Anchor::SAFE_ZONE_BOTTOM_RIGHT, {safeX + safeW, screenH}}
    };
}

void HudCompositor::recalculate() {
    m_cachedTransforms.clear();
    std::vector<Rect> placedRects;
    float scale = screenH / VIRT_H;

    for (auto& def : m_definitions) {
        Vector2 aPos = m_anchorTable.count(def.mappingAnchor) ? m_anchorTable[def.mappingAnchor] : Vector2{0,0};
        float fScale = scale * def.scale;

        Rect viewRect = {
            aPos.x + (def.mappingRect.x * scale),
            aPos.y + (def.mappingRect.y * scale),
            def.mappingRect.w * fScale,
            def.mappingRect.h * fScale
        };

        bool overlap = false;
        for (auto& existing : placedRects) {
            if (viewRect.intersects(existing)) {
                ERROR_LOG(RENDERER, "Overlap: Pruning zone '%s'", def.name.c_str());
                overlap = true; break;
            }
        }

        if (!overlap) {
            m_cachedTransforms.push_back({def.name, viewRect, def.mappingRect, def.zenMode});
            placedRects.push_back(viewRect);
        }

    }
}

Anchor HudCompositor::strToAnchor(const std::string& s) {
    static std::map<std::string, Anchor> l = {
        {"SCREEN_TOP_LEFT", Anchor::SCREEN_TOP_LEFT}, {"SCREEN_TOP_MID", Anchor::SCREEN_TOP_MID}, {"SCREEN_TOP_RIGHT", Anchor::SCREEN_TOP_RIGHT},
        {"SCREEN_LEFT_MID", Anchor::SCREEN_LEFT_MID}, {"SCREEN_CENTER", Anchor::SCREEN_CENTER}, {"SCREEN_RIGHT_MID", Anchor::SCREEN_RIGHT_MID},
        {"SCREEN_BOTTOM_LEFT", Anchor::SCREEN_BOTTOM_LEFT}, {"SCREEN_BOTTOM_MID", Anchor::SCREEN_BOTTOM_MID}, {"SCREEN_BOTTOM_RIGHT", Anchor::SCREEN_BOTTOM_RIGHT},
        {"SAFE_ZONE_TOP_LEFT", Anchor::SAFE_ZONE_TOP_LEFT}, {"SAFE_ZONE_TOP_MID", Anchor::SAFE_ZONE_TOP_MID}, {"SAFE_ZONE_TOP_RIGHT", Anchor::SAFE_ZONE_TOP_RIGHT},
        {"SAFE_ZONE_LEFT_MID", Anchor::SAFE_ZONE_LEFT_MID}, {"SAFE_ZONE_RIGHT_MID", Anchor::SAFE_ZONE_RIGHT_MID},
        {"SAFE_ZONE_BOTTOM_LEFT", Anchor::SAFE_ZONE_BOTTOM_LEFT}, {"SAFE_ZONE_BOTTOM_MID", Anchor::SAFE_ZONE_BOTTOM_MID}, {"SAFE_ZONE_BOTTOM_RIGHT", Anchor::SAFE_ZONE_BOTTOM_RIGHT}
    };
    return l.count(s) ? l[s] : Anchor::UNKNOWN;
}

bool HudCompositor::loadFromJson(const std::string& jsonStr, float vW, float vH) {
    try {
        auto data = nlohmann::json::parse(jsonStr);
        if (!data.contains("safe_zone") ||
            std::abs(data["safe_zone"].value("w", 0.0f) - VIRT_W) > 0.1f ||
            std::abs(data["safe_zone"].value("h", 0.0f) - VIRT_H) > 0.1f) {
            ERROR_LOG(RENDERER, "SafeZone mismatch (Expected 640x480)");
            return false;
        }

        m_definitions.clear();
        for (auto& j : data["hud_zones"]) {
            auto& m = j["mapping"];
            m_definitions.push_back({
                j.value("name", "unnamed"),
                { m["x"], m["y"], m["w"], m["h"] },
                strToAnchor(m.value("anchor", "SCREEN_TOP_LEFT")),
                m.value("scale", 1.0f),
                m.value("zen_mode", false)
            });
        }
        updateViewport(vW, vH);
        return true;
    } catch (...) { return false; }
}

void HudCompositor::updateViewport(float vW, float vH) {
    NOTICE_LOG(RENDERER, "HudCompositor::updateViewport: screenW=%.2f, screenH=%.2f, scale=%.4f", vW, vH, vH / VIRT_H);
    screenW = vW; screenH = vH;
    refreshAnchorTable();
    recalculate();
}
