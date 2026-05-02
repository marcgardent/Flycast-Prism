#pragma once
#include "TestCase.h"
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

class TestGEO01 : public TestCase {
public:
    std::string getId() const override { return "GEO-01"; }
    std::string getName() const override { return "Simple Square"; }
    std::string getDescription() const override { return "A simple opaque square (2 triangles)."; }

    void prepare(TestData& data) override {
        DrawBatch batch;
        batch.vertices.resize(4);
        float centerX = 320.0f;
        float centerY = 240.0f;
        float size = 100.0f;

        batch.vertices[0] = { centerX - size, centerY - size, 0.5f, {255, 255, 255, 255} };
        batch.vertices[1] = { centerX + size, centerY - size, 0.5f, {255, 255, 255, 255} };
        batch.vertices[2] = { centerX + size, centerY + size, 0.5f, {255, 255, 255, 255} };
        batch.vertices[3] = { centerX - size, centerY + size, 0.5f, {255, 255, 255, 255} };

        batch.indices = { 0, 1, 2, 0, 2, 3 };
        data.batches.push_back(batch);
    }
};

class TestGEO02 : public TestCase {
public:
    std::string getId() const override { return "GEO-02"; }
    std::string getName() const override { return "Culling"; }
    std::string getDescription() const override { return "Three overlapping triangles with different winding and culling."; }

    void prepare(TestData& data) override {
        // 1. Red Triangle: CCW, Cull None (Visible)
        {
            DrawBatch batch;
            batch.vertices.resize(3);
            batch.vertices[0] = { 320.0f, 100.0f, 0.5f, {255, 0, 0, 255} };
            batch.vertices[1] = { 420.0f, 300.0f, 0.5f, {255, 0, 0, 255} };
            batch.vertices[2] = { 220.0f, 300.0f, 0.5f, {255, 0, 0, 255} };
            batch.indices = { 0, 2, 1 }; // CCW
            batch.cullMode = FLYCAST_CULL_NONE;
            data.batches.push_back(batch);
        }

        // 2. Green Triangle: CCW, Cull Back (Visible if CCW=Front)
        {
            DrawBatch batch;
            batch.vertices.resize(3);
            batch.vertices[0] = { 320.0f, 150.0f, 0.4f, {0, 255, 0, 255} };
            batch.vertices[1] = { 420.0f, 350.0f, 0.4f, {0, 255, 0, 255} };
            batch.vertices[2] = { 220.0f, 350.0f, 0.4f, {0, 255, 0, 255} };
            batch.indices = { 0, 2, 1 }; // CCW
            batch.cullMode = FLYCAST_CULL_BACK;
            data.batches.push_back(batch);
        }

        // 3. Blue Triangle: CW, Cull Back (Hidden if CCW=Front)
        {
            DrawBatch batch;
            batch.vertices.resize(3);
            batch.vertices[0] = { 320.0f, 200.0f, 0.3f, {0, 0, 255, 255} };
            batch.vertices[1] = { 420.0f, 400.0f, 0.3f, {0, 0, 255, 255} };
            batch.vertices[2] = { 220.0f, 400.0f, 0.3f, {0, 0, 255, 255} };
            batch.indices = { 0, 1, 2 }; // CW
            batch.cullMode = FLYCAST_CULL_BACK;
            data.batches.push_back(batch);
        }
    }
};

class TestGEO03 : public TestCase {
public:
    std::string getId() const override { return "GEO-03"; }
    std::string getName() const override { return "Clipping (Scissor)"; }
    std::string getDescription() const override { return "A full-screen polygon with a 320x240 Scissor zone in the center."; }

    void prepare(TestData& data) override {
        DrawBatch batch;
        batch.vertices.resize(4);
        batch.vertices[0] = { 0.0f,   0.0f,   0.5f, {255, 255, 255, 255} };
        batch.vertices[1] = { 640.0f, 0.0f,   0.5f, {255, 255, 255, 255} };
        batch.vertices[2] = { 640.0f, 480.0f, 0.5f, {255, 255, 255, 255} };
        batch.vertices[3] = { 0.0f,   480.0f, 0.5f, {255, 255, 255, 255} };

        batch.indices = { 0, 1, 2, 0, 2, 3 };
        batch.scissorEnable = true;
        batch.scissorX = 160;
        batch.scissorY = 120;
        batch.scissorW = 320;
        batch.scissorH = 240;
        data.batches.push_back(batch);
    }
};

class TestSHD01 : public TestCase {
public:
    std::string getId() const override { return "SHD-01"; }
    std::string getName() const override { return "Interpolation (Gouraud)"; }
    std::string getDescription() const override { return "A triangle with Red/Green/Blue vertices."; }

    void prepare(TestData& data) override {
        DrawBatch batch;
        batch.vertices.resize(3);
        batch.vertices[0] = { 320.0f, 100.0f, 0.5f, {255, 0, 0, 255} };
        batch.vertices[1] = { 500.0f, 400.0f, 0.5f, {0, 255, 0, 255} };
        batch.vertices[2] = { 140.0f, 400.0f, 0.5f, {0, 0, 255, 255} };

        batch.indices = { 0, 1, 2 };
        data.batches.push_back(batch);
    }
};

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
    std::string getDescription() const override {
        return "An indexed 8BPP sprite with a rainbow palette buffer.";
    }

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
