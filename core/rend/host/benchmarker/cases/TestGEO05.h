#pragma once
#include "TestCommon.h"

/**
 * Test GEO-05: Alternating Strip Winding & Culling
 * This test submits a single triangle strip with 4 vertices (forming 2 triangles).
 * In a strip, the second triangle (Odd index) has reversed winding.
 * This test verifies if the renderer correctly applies the Even/Odd unrolling rule
 * to maintain consistent culling across the entire strip.
 */
class TestGEO05 : public TestCase {
public:
    std::string getId() const override { return "GEO-05"; }
    std::string getName() const override { return "Strip Winding & Culling"; }
    std::string getDescription() const override {
        return "Single strip with 4 vertices. Verifies Even/Odd winding unrolling for culling.";
    }
    std::string getExpected() const override {
        return "A single solid square (Red) composed of two triangles. If unrolling is wrong, one triangle will be culled and only half the square will appear.";
    }

    void prepare(TestData& data) override {
        DrawBatch batch;
        batch.cullMode = FLYCAST_CULL_BACK; // Should cull CW, keep CCW

        // Strip vertices:
        // 0: (100, 100) - Top-Left
        // 1: (100, 300) - Bottom-Left
        // 2: (300, 100) - Top-Right
        // 3: (300, 300) - Bottom-Right
        //
        // Triangle 0 (Even): (0, 1, 2) -> (TL, BL, TR) -> CCW (Visible)
        // Triangle 1 (Odd):  (1, 2, 3) -> (BL, TR, BR) -> CW (Would be culled if not unrolled correctly)
        // Correct unrolling for Triangle 1 should be (2, 1, 3) -> CCW.
        
        data.addStrip({
            { .x = 100.0f, .y = 100.0f, .z_inv = 0.5f, .col = {255, 0, 0, 255} },
            { .x = 100.0f, .y = 300.0f, .z_inv = 0.5f, .col = {255, 0, 0, 255} },
            { .x = 300.0f, .y = 100.0f, .z_inv = 0.5f, .col = {255, 0, 0, 255} },
            { .x = 300.0f, .y = 300.0f, .z_inv = 0.5f, .col = {255, 0, 0, 255} }
        }, batch);
    }
};
