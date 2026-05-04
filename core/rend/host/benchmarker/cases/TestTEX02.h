#pragma once
#include "TestCommon.h"
#include <cstring>

extern uint32_t palette32_ram[1024];

/**
 * Test TEX-02: UV Orientation Verification
 * 
 * Verifies that V=0 is at the TOP and V=1 is at the BOTTOM.
 * A 2x2 quadrant texture is used:
 * - Top-Left: RED
 * - Top-Right: GREEN
 * - Bottom-Left: BLUE
 * - Bottom-Right: YELLOW
 */
class TestTEX02 : public TestCase {
public:
    std::string getId() const override { return "TEX-02"; }
    std::string getName() const override { return "UV Orientation"; }
    std::string getDescription() const override { return "Verifies UV orientation mapping. Expected: Red (Top-Left), Green (Top-Right), Blue (Bottom-Left), Yellow (Bottom-Right)."; }
    std::string getExpected() const override { return "Four colored quadrants: Red (Top-Left), Green (Top-Right), Blue (Bottom-Left), Yellow (Bottom-Right). If the V coordinate is incorrectly flipped, Blue and Yellow will appear at the top."; }

    void prepare(TestData& data) override {
        constexpr uint32_t TEX_W = 256;
        constexpr uint32_t TEX_H = 256;

        // Palette in RGBA8 (R, G, B, A in memory)
        std::vector<uint32_t> pal(1024, 0);
        pal[0] = 0xFF000000; // Black
        pal[1] = 0xFF0000FF; // Red
        pal[2] = 0xFF00FF00; // Green
        pal[3] = 0xFFFF0000; // Blue
        pal[4] = 0xFF00FFFF; // Yellow
        
        memcpy(palette32_ram, pal.data(), 1024 * sizeof(uint32_t));

        std::vector<uint8_t> tex(TEX_W * TEX_H);
        for (uint32_t y = 0; y < TEX_H; ++y) {
            for (uint32_t x = 0; x < TEX_W; ++x) {
                int quadrant;
                if (y < TEX_H / 2) {
                    quadrant = (x < TEX_W / 2) ? 1 : 2; // Top: Red(L), Green(R)
                } else {
                    quadrant = (x < TEX_W / 2) ? 3 : 4; // Bottom: Blue(L), Yellow(R)
                }
                tex[y * TEX_W + x] = (uint8_t)quadrant;
            }
        }

        DrawBatch batch;
        batch.texMode   = FLYCAST_TEX_PAL8;
        batch.texWidth  = TEX_W;
        batch.texHeight = TEX_H;
        batch.texData   = std::move(tex);
        batch.palette   = std::move(pal);

        data.addStrip({
            { 0.0f,   0.0f,   0.5f, {255, 255, 255, 255}, {0,0,0,0}, 0.0f, 0.0f }, // TL
            { 640.0f, 0.0f,   0.5f, {255, 255, 255, 255}, {0,0,0,0}, 1.0f, 0.0f }, // TR
            { 0.0f,   480.0f, 0.5f, {255, 255, 255, 255}, {0,0,0,0}, 0.0f, 1.0f }, // BL
            { 640.0f, 480.0f, 0.5f, {255, 255, 255, 255}, {0,0,0,0}, 1.0f, 1.0f }  // BR
        }, batch);
    }
};
