#pragma once
#include "TestCommon.h"

/**
 * Mandatory conformity test for the renderer.
 * Validates the Y-Down axis, Z-Inverse depth, and PVR Culling.
 */
class TestGEO02 : public TestCase {
public:
    std::string getId() const override { return "GEO-02"; }
    std::string getName() const override { return "PVR Conformity (Z-Inv/Cull)"; }
    std::string getDescription() const override { return "Validates Y-Down axis, Z-Inverse depth and culling (Back = CW)."; }

    std::string getExpected() const override {
        return "Three triangles in total: Green in front of Red (on the left), and Blue alone (on the right). The Yellow triangle must be hidden by culling (CW).";
    }

    void prepare(TestData& data) override {
        // --- 1. Z-Inverse Test (PVR) ---
        {
            DrawBatch b1; // Green (Near, drawn 1st)
            b1.cullMode = FLYCAST_CULL_NONE;
            b1.depthFunc = FLYCAST_DEPTH_GREATER;
            data.addStrip({
                { 150.0f, 150.0f, 0.8f, {0, 255, 0, 255} },
                { 350.0f, 150.0f, 0.8f, {0, 255, 0, 255} },
                { 250.0f, 350.0f, 0.8f, {0, 255, 0, 255} }
            }, b1);

            DrawBatch b2; // Red (Far, drawn 2nd)
            b2.cullMode = FLYCAST_CULL_NONE;
            b2.depthFunc = FLYCAST_DEPTH_GREATER;
            data.addStrip({
                { 100.0f, 100.0f, 0.2f, {255, 0, 0, 255} },
                { 300.0f, 100.0f, 0.2f, {255, 0, 0, 255} },
                { 200.0f, 300.0f, 0.2f, {255, 0, 0, 255} }
            }, b2);
        }

        // --- 2. Culling Test (Winding) ---
        {
            DrawBatch b3; // Blue (CCW)
            b3.cullMode = FLYCAST_CULL_BACK;
            data.addStrip({
                { 400.0f, 100.0f, 0.5f, {0, 0, 255, 255} },
                { 300.0f, 300.0f, 0.5f, {0, 0, 255, 255} },
                { 500.0f, 300.0f, 0.5f, {0, 0, 255, 255} }
            }, b3);

            DrawBatch b4; // Yellow (CW)
            b4.cullMode = FLYCAST_CULL_BACK;
            data.addStrip({
                { 400.0f, 200.0f, 0.5f, {255, 255, 0, 255} },
                { 500.0f, 400.0f, 0.5f, {255, 255, 0, 255} },
                { 300.0f, 400.0f, 0.5f, {255, 255, 0, 255} }
            }, b4);
        }
    }
};