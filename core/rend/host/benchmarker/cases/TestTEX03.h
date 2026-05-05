#pragma once
#include "TestCommon.h"
#include <cstring>

/**
 * Test TEX-03: Perspective-Correct Texture Mapping
 * * Draws a quad that is deeply tilted into the Z-axis with a high-contrast checkerboard texture.
 * This test verifies if the renderer implements perspective-correct UV interpolation
 * (interpolating U/W, V/W, and 1/W instead of linear screen-space U, V).
 * * Trap: If interpolation is affine, the checkerboard squares will look bent and
 * a visible discontinuity (kink) will appear along the quad's diagonal.
 */
class TestTEX03 : public TestCase {
public:
    std::string getId() const override { return "TEX-03"; }
    std::string getName() const override { return "Perspective Texture Mapping"; }
    std::string getDescription() const override {
        return "Tilted quad with a 8x8 checkerboard. Stress-tests perspective-correct UV interpolation.";
    }
    std::string getExpected() const override {
        return "A perfectly straight checkerboard receding into the distance. All grid lines must be straight without any 'bending' or 'kinks' at the diagonal.";
    }

    void prepare(TestData& data) override {
        constexpr uint32_t TEX_W = 256;
        constexpr uint32_t TEX_H = 256;

        // Palette in RGBA8: Index 0 = Black, Index 1 = White
        std::vector<uint32_t> pal(1024, 0);
        pal[0] = 0xFF000000; // Black
        pal[1] = 0xFFFFFFFF; // White

        // Update the hardware palette bank (benchmarker simulation)
        extern uint32_t palette32_ram[1024];
        memcpy(palette32_ram, pal.data(), 1024 * sizeof(uint32_t));

        // Generate 8x8 Checkerboard (32x32 blocks)
        std::vector<uint8_t> tex(TEX_W * TEX_H);
        for (uint32_t y = 0; y < TEX_H; ++y) {
            for (uint32_t x = 0; x < TEX_W; ++x) {
                bool is_white = ((x / 32) + (y / 32)) % 2 == 0;
                tex[y * TEX_W + x] = is_white ? 1 : 0;
            }
        }

        DrawBatch batch;
        batch.texMode   = FLYCAST_TEX_PAL8;
        batch.texWidth  = TEX_W;
        batch.texHeight = TEX_H;
        batch.texData   = std::move(tex);
        batch.palette   = std::move(pal);
        batch.cullMode  = FLYCAST_CULL_NONE;

        // Geometry: A mathematically correct deep trapezoid in screen space.
        // Near z_inv=0.9, Far z_inv=0.1 (Ratio of 9).
        // The screen width MUST also have a ratio of 9 to form a valid flat 3D plane!
        // Top width  = 50  (345 - 295)
        // Bot width  = 450 (545 - 95)
        data.addStrip({
            // { x,      y,      z,     color,               offset, u,    v }
            {  95.0f,  450.0f, 0.9f, {255, 255, 255, 255}, {0,0,0,0}, 0.0f, 1.0f }, // Bottom-Left (Near)
            { 545.0f,  450.0f, 0.9f, {255, 255, 255, 255}, {0,0,0,0}, 1.0f, 1.0f }, // Bottom-Right (Near)
            { 295.0f,  150.0f, 0.1f, {255, 255, 255, 255}, {0,0,0,0}, 0.0f, 0.0f }, // Top-Left (Far)
            { 345.0f,  150.0f, 0.1f, {255, 255, 255, 255}, {0,0,0,0}, 1.0f, 0.0f }  // Top-Right (Far)
        }, batch);
    }
};