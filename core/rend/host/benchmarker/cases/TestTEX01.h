#pragma once

#include "TestCommon.h"


// ============================================================================
// TEX-01 : Palette Lookup (8BPP)
// A full-screen quad textured with a 256x256 8BPP indexed texture.
// Each texel index = (x + y) & 0xFF -> 256 diagonal rainbow bands.
// Palette: 256 entries, HSL rainbow (hue = i/256 * 360 degrees).
// Expected: smooth diagonal rainbow gradient across the screen.
// ============================================================================
class TestTEX01 : public TestCase {
public:
    std::string getId() const override { return "TEX-01"; }
    std::string getName() const override { return "Palette Lookup (8BPP)"; }
    std::string getDescription() const override { return "Texture mapping with palette lookup (8BPP). Each texel is an index into a 256-color palette."; }
    std::string getExpected() const override { return "A full-screen rainbow gradient (Z=0.5) using an 8BPP indexed texture."; }

    void prepare(TestData& data) override {
        constexpr uint32_t TEX_W = 256;
        constexpr uint32_t TEX_H = 256;

        // ---- Build rainbow palette (256 ARGB32 entries) ----
        std::vector<uint32_t> pal(256);
        for (int i = 0; i < 256; ++i) {
            float hue = (float)i / 256.0f * 360.0f;
            pal[i] = hsl_to_argb32(hue, 1.0f, 0.5f);
        }

        // ---- Build 8BPP texture: index = (x + y) & 0xFF ----
        // Produces 256 diagonal bands, each mapped to a distinct palette color.
        std::vector<uint8_t> tex(TEX_W * TEX_H);
        for (uint32_t y = 0; y < TEX_H; ++y) {
            for (uint32_t x = 0; x < TEX_W; ++x) {
                tex[y * TEX_W + x] = (uint8_t)((x + y) & 0xFF);
            }
        }

        // ---- Full-screen quad with UV (0,0) -> (1,1) ----
        DrawBatch batch;
        batch.vertices.resize(4);

        // Top-left
        batch.vertices[0].x = 0.0f;   batch.vertices[0].y = 0.0f;   batch.vertices[0].z = 0.5f;
        batch.vertices[0].col[0] = 255; batch.vertices[0].col[1] = 255;
        batch.vertices[0].col[2] = 255; batch.vertices[0].col[3] = 255;
        batch.vertices[0].u = 0.0f;    batch.vertices[0].v = 0.0f;

        // Top-right
        batch.vertices[1].x = 640.0f; batch.vertices[1].y = 0.0f;   batch.vertices[1].z = 0.5f;
        batch.vertices[1].col[0] = 255; batch.vertices[1].col[1] = 255;
        batch.vertices[1].col[2] = 255; batch.vertices[1].col[3] = 255;
        batch.vertices[1].u = 1.0f;    batch.vertices[1].v = 0.0f;

        // Bottom-right
        batch.vertices[2].x = 640.0f; batch.vertices[2].y = 480.0f; batch.vertices[2].z = 0.5f;
        batch.vertices[2].col[0] = 255; batch.vertices[2].col[1] = 255;
        batch.vertices[2].col[2] = 255; batch.vertices[2].col[3] = 255;
        batch.vertices[2].u = 1.0f;    batch.vertices[2].v = 1.0f;

        // Bottom-left
        batch.vertices[3].x = 0.0f;   batch.vertices[3].y = 480.0f; batch.vertices[3].z = 0.5f;
        batch.vertices[3].col[0] = 255; batch.vertices[3].col[1] = 255;
        batch.vertices[3].col[2] = 255; batch.vertices[3].col[3] = 255;
        batch.vertices[3].u = 0.0f;    batch.vertices[3].v = 1.0f;

        batch.indices = { 0, 1, 2, 0, 2, 3 };

        batch.texMode   = FLYCAST_TEX_PAL8;
        batch.texWidth  = TEX_W;
        batch.texHeight = TEX_H;
        batch.texData   = std::move(tex);
        batch.palette   = std::move(pal);

        data.batches.push_back(std::move(batch));
    }
};
