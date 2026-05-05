#pragma once
#include "TestCommon.h"

/**
 * Test GEO-10: HUD High-Precision Z-Fighting
 * Simulates HUD flickering issues by using extreme depth values (near 1.0)
 * and a sub-microscopic depth sweep.
 */
class TestGEO10 : public TestCase {
    float time = 0.0f;
public:
    std::string getId() const override { return "GEO-10"; }
    std::string getName() const override { return "HUD High-Precision Z-Fighting"; }
    std::string getDescription() const override {
        return "Renders two quads at extreme depths (Z ≈ 0.99999) with a tiny depth sweep. "
               "Tests Z-buffer stability for HUD elements that often occupy the same depth plane.";
    }
    std::string getExpected() const override {
        return "The Magenta quad should remain mostly stable over the White background. "
               "Intense flickering indicates a precision issue in the depth pipeline.";
    }

    uint32_t getFrameCount() const override { return 600; }

    void update(float dt) override {
        time += dt;
    }

    void prepare(TestData& data) override {
        float centerX = 320.0f;
        float centerY = 240.0f;
        float size = 200.0f;

        // Base depth at the limit of float32 precision for 1.0 range
        float base_z = 0.99999f;
        
        // Extremely slow oscillation: 0.0 to 0.00001 range
        float sweep = (std::sin(time * 0.5f) * 0.5f + 0.5f) * 0.00001f;

        // 1. Background "HUD" (White)
        data.addStrip({
            { .x = centerX - size, .y = centerY - size, .z_inv = base_z, .col = {255, 255, 255, 255} },
            { .x = centerX + size, .y = centerY - size, .z_inv = base_z, .col = {255, 255, 255, 255} },
            { .x = centerX - size, .y = centerY + size, .z_inv = base_z, .col = {255, 255, 255, 255} },
            { .x = centerX + size, .y = centerY + size, .z_inv = base_z, .col = {255, 255, 255, 255} }
        });

        // 2. Foreground "HUD" (Magenta)
        data.addStrip({
            { .x = centerX - size + 20, .y = centerY - size + 20, .z_inv = base_z + sweep, .col = {255, 0, 255, 255} },
            { .x = centerX + size - 20, .y = centerY - size + 20, .z_inv = base_z + sweep, .col = {255, 0, 255, 255} },
            { .x = centerX - size + 20, .y = centerY + size - 20, .z_inv = base_z + sweep, .col = {255, 0, 255, 255} },
            { .x = centerX + size - 20, .y = centerY + size - 20, .z_inv = base_z + sweep, .col = {255, 0, 255, 255} }
        });
    }
};
