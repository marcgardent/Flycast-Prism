#pragma once
#include "TestCommon.h"

/**
 * Test GEO-09: Z-Fighting Stability
 * Renders multiple overlapping quads at near-identical depths to test depth buffer precision and stability.
 * Uses 10 passes per frame and runs for 300 frames.
 */
class TestGEO09 : public TestCase {
    float time = 0.0f;
public:
    std::string getId() const override { return "GEO-09"; }
    std::string getName() const override { return "Z-Fighting Stability"; }
    std::string getDescription() const override {
        return "Renders 10 overlapping quads (alternating Red/Green) at near-identical depths. "
               "The green quads oscillate very slightly around Z=0.5 to test depth stability over 300 frames.";
    }
    std::string getExpected() const override {
        return "A stable rendering without heavy flickering. If Z-fighting or floating-point instability occurs, "
               "the red and green colors will fight, showing noise patterns.";
    }

    uint32_t getFrameCount() const override { return 300; }

    void update(float dt) override {
        time += dt;
    }

    void prepare(TestData& data) override {
        float centerX = 320.0f;
        float centerY = 240.0f;
        float size = 180.0f;

        // Base depth (1/W)
        float base_z = 0.5f;
        
        // Very tiny oscillation to stress depth precision
        float offset = std::sin(time * 5.0f) * 0.000001f;

        // Multiple passes to check accumulation and depth consistency
        for (int i = 0; i < 5; ++i) {
            // Red Quad (Z = 0.5)
            data.addStrip({
                { .x = centerX - size, .y = centerY - size, .z_inv = base_z, .col = {255, 0, 0, 255} },
                { .x = centerX + size, .y = centerY - size, .z_inv = base_z, .col = {255, 0, 0, 255} },
                { .x = centerX - size, .y = centerY + size, .z_inv = base_z, .col = {255, 0, 0, 255} },
                { .x = centerX + size, .y = centerY + size, .z_inv = base_z, .col = {255, 0, 0, 255} }
            });

            // Green Quad (Z = 0.5 + tiny offset)
            data.addStrip({
                { .x = centerX - size, .y = centerY - size, .z_inv = base_z + offset, .col = {0, 255, 0, 255} },
                { .x = centerX + size, .y = centerY - size, .z_inv = base_z + offset, .col = {0, 255, 0, 255} },
                { .x = centerX - size, .y = centerY + size, .z_inv = base_z + offset, .col = {0, 255, 0, 255} },
                { .x = centerX + size, .y = centerY + size, .z_inv = base_z + offset, .col = {0, 255, 0, 255} }
            });
        }
    }
};
