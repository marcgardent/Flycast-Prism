#include "HudCompositor.h"
#include <cmath>
#include "log/Log.h"
#include "json.hpp"

bool Rect::intersects(const Rect& o) const {
    return (x < o.x + o.w && x + w > o.x && y < o.y + o.h && y + h > o.y);
}

void HudCompositor::refreshAnchorTable() {
    float scale = screenH / VIRT_H;
    float safeW = VIRT_W * scale;
    float safeX = (screenW - safeW) / 2.0f;

    m_anchorTable = {
        {Anchor::SCREEN_TOP_LEFT,      {0, 0}},
        {Anchor::SCREEN_TOP_MID,       {screenW / 2.0f, 0}},
        {Anchor::SCREEN_TOP_RIGHT,     {screenW, 0}},
        {Anchor::SCREEN_LEFT_MID,      {0, screenH / 2.0f}},
        {Anchor::SCREEN_CENTER,        {screenW / 2.0f, screenH / 2.0f}},
        {Anchor::SCREEN_RIGHT_MID,     {screenW, screenH / 2.0f}},
        {Anchor::SCREEN_BOTTOM_LEFT,   {0, screenH}},
        {Anchor::SCREEN_BOTTOM_MID,    {screenW / 2.0f, screenH}},
        {Anchor::SCREEN_BOTTOM_RIGHT,  {screenW, screenH}},
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
        // 1. Calculate real coordinates for the SOURCE
        Vector2 srcAnchorPos = m_anchorTable.count(def.sourceAnchor) ? m_anchorTable[def.sourceAnchor] : Vector2{0,0};
        Rect realSource = {
            srcAnchorPos.x + (def.sourceRect.x * scale),
            srcAnchorPos.y + (def.sourceRect.y * scale),
            def.sourceRect.w * scale,
            def.sourceRect.h * scale
        };

        // 2. Calculate real coordinates for the MAPPING (destination)
        Vector2 mapAnchorPos = m_anchorTable.count(def.mappingAnchor) ? m_anchorTable[def.mappingAnchor] : Vector2{0,0};
        Rect realMapping = {
            mapAnchorPos.x + (def.destRect.x * scale),
            mapAnchorPos.y + (def.destRect.y * scale),
            def.destRect.w * scale,
            def.destRect.h * scale
        };

        // 3. Check for collisions on the destination
        bool overlap = false;
        for (auto& existing : placedRects) {
            if (realMapping.intersects(existing)) {
                ERROR_LOG(RENDERER, "Overlap: Pruning zone '%s'", def.name.c_str());
                overlap = true; break;
            }
        }

        if (!overlap) {
            m_cachedTransforms.push_back({def.name, realSource, realMapping, def.zenMode});
            placedRects.push_back(realMapping);
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
            ERROR_LOG(RENDERER, "SafeZone mismatch (Expected %.0fx%.0f)", VIRT_W, VIRT_H);
            return false;
        }

        m_definitions.clear();
        for (auto& j : data["hud_zones"]) {
            if (!j.contains("source") || !j.contains("mapping")) {
                ERROR_LOG(RENDERER, "Missing source or mapping in zone %s", j.value("name", "unnamed").c_str());
                continue;
            }

            auto& s = j["source"];
            auto& m = j["mapping"];
            std::string zoneName = j.value("name", "unnamed");

            float sW = s.value("w", 0.0f);
            float sH = s.value("h", 0.0f);
            float mW = m.value("w", 0.0f);
            float mH = m.value("h", 0.0f);

            // Source and destination must have the same dimensions
            if (std::abs(sW - mW) > 0.001f || std::abs(sH - mH) > 0.001f) {
                ERROR_LOG(RENDERER, "Dimension mismatch between source and mapping for zone '%s'. Pruning.", zoneName.c_str());
                continue;
            }

            m_definitions.push_back({
                zoneName,
                { s.value("x", 0.0f), s.value("y", 0.0f), sW, sH },
                strToAnchor(s.value("anchor", "SCREEN_TOP_LEFT")),
                { m.value("x", 0.0f), m.value("y", 0.0f), mW, mH },
                strToAnchor(m.value("anchor", "SCREEN_TOP_LEFT")),
                m.value("zen_mode", false)
            });
        }
        updateViewport(vW, vH);
        return true;
    } catch (...) {
        return false;
    }
}

void HudCompositor::updateViewport(float vW, float vH) {
    NOTICE_LOG(RENDERER, "HudCompositor::updateViewport: screenW=%.2f, screenH=%.2f, scale=%.4f", vW, vH, vH / VIRT_H);
    screenW = vW; screenH = vH;
    refreshAnchorTable();
    recalculate();
}