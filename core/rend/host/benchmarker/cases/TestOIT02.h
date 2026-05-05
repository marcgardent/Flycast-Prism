#pragma once
#include "TestCommon.h"

/**
 * Test OIT-02: Translucent Z-Fighting (Vicious Sweep)
 * Renders overlapping translucent quads with sub-microscopic depth variations.
 * Stresses OIT sorting and accumulation stability by forcing depth-reversals.
 */
class TestOIT02 : public TestCase {
    float time = 0.0f;
public:
    std::string getId() const override { return "OIT-02"; }
    std::string getName() const override { return "Translucent Z-Fighting (Sweep)"; }
    std::string getDescription() const override {
        return "Vicious OIT test: 4 translucent quads at Z ≈ 0.5 with multiple sub-microscopic depth sweeps. "
               "Forces fragments to cross each other's depth planes repeatedly.";
    }
    std::string getExpected() const override {
        return "A stable, blended color mixture. No flickering or sorting 'pops' should occur during the sweep.";
    }

    uint32_t getFrameCount() const override { return 600; }

    void update(float dt) override {
        time += dt;
    }

    void prepare(TestData& data) override {
        auto addQuad = [&](float x, float y, float z, uint32_t rgba) {
            DrawBatch batch;
            batch.srcBlend = FLYCAST_BLEND_SRC_ALPHA;
            batch.dstBlend = FLYCAST_BLEND_INV_SRC_ALPHA;
            batch.depthWrite = false;
            batch.depthFunc = FLYCAST_DEPTH_GEQUAL; 
            batch.listType = FLYCAST_LIST_TRANSLUCENT;

            float size = 250.0f;
            uint8_t r = (rgba >> 0) & 0xFF;
            uint8_t g = (rgba >> 8) & 0xFF;
            uint8_t b = (rgba >> 16) & 0xFF;
            uint8_t a = (rgba >> 24) & 0xFF;

            data.addStrip({
                { .x = x, .y = y, .z_inv = z, .col = {r, g, b, a} },
                { .x = x + size, .y = y, .z_inv = z, .col = {r, g, b, a} },
                { .x = x, .y = y + size, .z_inv = z, .col = {r, g, b, a} },
                { .x = x + size, .y = y + size, .z_inv = z, .col = {r, g, b, a} }
            }, batch);
        };

        float base_z = 0.5f;
        // Sub-microscopic sweep (same scale as GEO-10)
        float sweep = (std::sin(time * 0.5f) * 0.5f + 0.5f) * 0.00001f;

        // 4 Layers with different Z-trajectories to force sorting reversals
        addQuad(200.0f, 150.0f, base_z,                   0x800000FF); // Red (Fixed)
        addQuad(230.0f, 180.0f, base_z + sweep,           0x8000FF00); // Green (Sweep Up)
        addQuad(260.0f, 210.0f, base_z + (0.00001f - sweep), 0x80FF0000); // Blue (Sweep Down)
        addQuad(290.0f, 240.0f, base_z + sweep * 0.5f,     0x8000FFFF); // Yellow (Slow Sweep)
    }
};
