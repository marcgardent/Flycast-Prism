#pragma once
#include "TestCommon.h"

/**
 * Test GEO-08: Front-Face Culling (CCW)
 * Verifies that the renderer correctly culls front-facing triangles (CCW)
 * when FLYCAST_CULL_FRONT is enabled.
 */
class TestGEO08 : public TestCase {
public:
    std::string getId() const override { return "GEO-08"; }
    std::string getName() const override { return "Front-Face Culling"; }
    std::string getDescription() const override {
        return "Tests FLYCAST_CULL_FRONT (CCW culling).";
    }
    std::string getExpected() const override {
        return "A Yellow triangle (CW) should be visible. A Blue triangle (CCW) should be culled (hidden).";
    }

    void prepare(TestData& data) override {
        DrawBatch batch;
        batch.cullMode = FLYCAST_CULL_FRONT; // Cull CCW, keep CW

        // 1. Blue Triangle (CCW) -> Should be CULLED
        data.addStrip({
            { .x = 200.0f, .y = 100.0f, .z_inv = 0.5f, .col = {0, 0, 255, 255} },
            { .x = 100.0f, .y = 300.0f, .z_inv = 0.5f, .col = {0, 0, 255, 255} },
            { .x = 300.0f, .y = 300.0f, .z_inv = 0.5f, .col = {0, 0, 255, 255} }
        }, batch);

        // 2. Yellow Triangle (CW) -> Should be VISIBLE
        data.addStrip({
            { .x = 500.0f, .y = 100.0f, .z_inv = 0.5f, .col = {255, 255, 0, 255} },
            { .x = 600.0f, .y = 300.0f, .z_inv = 0.5f, .col = {255, 255, 0, 255} },
            { .x = 400.0f, .y = 300.0f, .z_inv = 0.5f, .col = {255, 255, 0, 255} }
        }, batch);
    }
};
