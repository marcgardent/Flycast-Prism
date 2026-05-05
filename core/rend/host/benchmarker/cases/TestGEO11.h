#pragma once
#include "TestCommon.h"

/**
 * Test GEO-11: Solid vs Translucent Z-Fighting
 * Validates depth stability between Opaque and Translucent passes.
 * Renders an Opaque White quad and a Translucent Magenta quad at near-identical depths.
 */
class TestGEO11 : public TestCase {
    float time = 0.0f;
public:
    std::string getId() const override { return "GEO-11"; }
    std::string getName() const override { return "Solid vs Translucent Z-Fighting"; }
    std::string getDescription() const override {
        return "Vicious Z-fighting test: Opaque (White) vs Translucent (Magenta). "
               "Both oscillate around Z ≈ 0.5 to test pass-to-pass depth precision.";
    }
    std::string getExpected() const override {
        return "The Translucent Magenta quad should smoothly blend over the White background. "
               "Flickering or 'poking through' indicates a depth mismatch between passes.";
    }

    uint32_t getFrameCount() const override { return 600; }

    void update(float dt) override {
        time += dt;
    }

    void prepare(TestData& data) override {
        float centerX = 320.0f;
        float centerY = 240.0f;
        float size = 300.0f;

        float base_z = 0.5f;
        float sweep = (std::sin(time * 0.5f) * 0.5f + 0.5f) * 0.00001f;

        // 1. Opaque Quad (White)
        {
            DrawBatch batch;
            batch.listType = FLYCAST_LIST_OPAQUE;
            batch.depthWrite = true;
            batch.depthFunc = FLYCAST_DEPTH_GEQUAL;

            data.addStrip({
                { .x = centerX - size, .y = centerY - size, .z_inv = base_z, .col = {255, 255, 255, 255} },
                { .x = centerX + size, .y = centerY - size, .z_inv = base_z, .col = {255, 255, 255, 255} },
                { .x = centerX - size, .y = centerY + size, .z_inv = base_z, .col = {255, 255, 255, 255} },
                { .x = centerX + size, .y = centerY + size, .z_inv = base_z, .col = {255, 255, 255, 255} }
            }, batch);
        }

        // 2. Translucent Quad (Magenta, 50% Alpha)
        {
            DrawBatch batch;
            batch.listType = FLYCAST_LIST_TRANSLUCENT;
            batch.srcBlend = FLYCAST_BLEND_SRC_ALPHA;
            batch.dstBlend = FLYCAST_BLEND_INV_SRC_ALPHA;
            batch.depthWrite = false;
            batch.depthFunc = FLYCAST_DEPTH_GEQUAL;

            float innerSize = size * 0.8f;
            data.addStrip({
                { .x = centerX - innerSize, .y = centerY - innerSize, .z_inv = base_z + sweep, .col = {255, 0, 255, 128} },
                { .x = centerX + innerSize, .y = centerY - innerSize, .z_inv = base_z + sweep, .col = {255, 0, 255, 128} },
                { .x = centerX - innerSize, .y = centerY + innerSize, .z_inv = base_z + sweep, .col = {255, 0, 255, 128} },
                { .x = centerX + innerSize, .y = centerY + innerSize, .z_inv = base_z + sweep, .col = {255, 0, 255, 128} }
            }, batch);
        }
    }
};
