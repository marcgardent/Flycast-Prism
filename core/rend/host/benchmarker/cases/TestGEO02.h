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
                { .x = 150.0f, .y = 150.0f, .z_inv = 0.8f, .col = {0, 255, 0, 255} },
                { .x = 350.0f, .y = 150.0f, .z_inv = 0.8f, .col = {0, 255, 0, 255} },
                { .x = 250.0f, .y = 350.0f, .z_inv = 0.8f, .col = {0, 255, 0, 255} }
            }, b1);

            DrawBatch b2; // Red (Far, drawn 2nd)
            b2.cullMode = FLYCAST_CULL_NONE;
            b2.depthFunc = FLYCAST_DEPTH_GREATER;
            data.addStrip({
                { .x = 100.0f, .y = 100.0f, .z_inv = 0.2f, .col = {255, 0, 0, 255} },
                { .x = 300.0f, .y = 100.0f, .z_inv = 0.2f, .col = {255, 0, 0, 255} },
                { .x = 200.0f, .y = 300.0f, .z_inv = 0.2f, .col = {255, 0, 0, 255} }
            }, b2);
        }

        // --- 2. Culling Test (Winding) ---
        {
            DrawBatch b3; // Blue (CCW)
            b3.cullMode = FLYCAST_CULL_BACK;
            data.addStrip({
                { .x = 400.0f, .y = 100.0f, .z_inv = 0.5f, .col = {0, 0, 255, 255} },
                { .x = 300.0f, .y = 300.0f, .z_inv = 0.5f, .col = {0, 0, 255, 255} },
                { .x = 500.0f, .y = 300.0f, .z_inv = 0.5f, .col = {0, 0, 255, 255} }
            }, b3);

            DrawBatch b4; // Yellow (CW)
            b4.cullMode = FLYCAST_CULL_BACK;
            data.addStrip({
                { .x = 400.0f, .y = 200.0f, .z_inv = 0.5f, .col = {255, 255, 0, 255} },
                { .x = 500.0f, .y = 400.0f, .z_inv = 0.5f, .col = {255, 255, 0, 255} },
                { .x = 300.0f, .y = 400.0f, .z_inv = 0.5f, .col = {255, 255, 0, 255} }
            }, b4);
        }
    }
};