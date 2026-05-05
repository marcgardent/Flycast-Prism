#pragma once
#include "TestCommon.h"
#include <limits>

/**
 * Test GEO-06: NaN & Infinity Safety
 * Submits triangles with invalid coordinates (NaN and Inf).
 * Verifies that the renderer handles these safely (culls them) without crashing
 * or corrupting the rest of the frame.
 */
class TestGEO06 : public TestCase {
public:
    std::string getId() const override { return "GEO-06"; }
    std::string getName() const override { return "NaN & Inf Safety"; }
    std::string getDescription() const override {
        return "Submits triangles with NaN/Inf coordinates mixed with valid geometry.";
    }
    std::string getExpected() const override {
        return "Two Green squares (Left and Right). The triangles in the middle (containing NaN/Inf) must be invisible and not cause any visual artifacts or crashes.";
    }

    void prepare(TestData& data) override {
        float nan = std::numeric_limits<float>::quiet_NaN();
        float inf = std::numeric_limits<float>::infinity();

        DrawBatch batch;
        batch.cullMode = FLYCAST_CULL_NONE;

        // 1. Valid Left Square
        data.addStrip({
            { 50.0f, 100.0f, 0.5f, {0, 255, 0, 255} },
            { 50.0f, 200.0f, 0.5f, {0, 255, 0, 255} },
            { 150.0f, 100.0f, 0.5f, {0, 255, 0, 255} },
            { 150.0f, 200.0f, 0.5f, {0, 255, 0, 255} }
        }, batch);

        // 2. NaN Triangle
        data.addStrip({
            { 200.0f, 100.0f, 0.5f, {255, 0, 0, 255} },
            { nan,    150.0f, 0.5f, {255, 0, 0, 255} },
            { 300.0f, 100.0f, 0.5f, {255, 0, 0, 255} }
        }, batch);

        // 3. Infinity Triangle
        data.addStrip({
            { 300.0f, 100.0f, 0.5f, {0, 0, 255, 255} },
            { inf,    inf,    0.5f, {0, 0, 255, 255} },
            { 400.0f, 100.0f, 0.5f, {0, 0, 255, 255} }
        }, batch);

        // 4. Valid Right Square
        data.addStrip({
            { 450.0f, 100.0f, 0.5f, {0, 255, 0, 255} },
            { 450.0f, 200.0f, 0.5f, {0, 255, 0, 255} },
            { 550.0f, 100.0f, 0.5f, {0, 255, 0, 255} },
            { 550.0f, 200.0f, 0.5f, {0, 255, 0, 255} }
        }, batch);
    }
};
