#pragma once
#include <cmath>

// ============================================================================
// Helper : HSL to ARGB32
// h in [0,360), s and l in [0,1]. Returns 0xAARRGGBB with A=0xFF.
// ============================================================================
static inline uint32_t hsl_to_argb32(float h, float s, float l) {
    auto hue2rgb = [](float p, float q, float t) -> float {
        if (t < 0.0f) t += 1.0f;
        if (t > 1.0f) t -= 1.0f;
        if (t < 1.0f/6.0f) return p + (q - p) * 6.0f * t;
        if (t < 1.0f/2.0f) return q;
        if (t < 2.0f/3.0f) return p + (q - p) * (2.0f/3.0f - t) * 6.0f;
        return p;
    };
    float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
    float p = 2.0f * l - q;
    float hn = h / 360.0f;
    uint8_t r = (uint8_t)(hue2rgb(p, q, hn + 1.0f/3.0f) * 255.0f + 0.5f);
    uint8_t g = (uint8_t)(hue2rgb(p, q, hn)              * 255.0f + 0.5f);
    uint8_t b = (uint8_t)(hue2rgb(p, q, hn - 1.0f/3.0f) * 255.0f + 0.5f);
    // ARGB32: A in MSB
    return (0xFFu << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
}




