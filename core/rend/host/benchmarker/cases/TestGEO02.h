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
        // We draw the GREEN (Near) first, then the RED (Far) second.
        // If the inverse Z-Buffer (GREATER) works correctly,
        // the Red should not overwrite the Green when drawn.
        {
            DrawBatch b1; // Green (Near, drawn 1st)
            b1.vertices = {
                { 150.0f, 150.0f, 0.8f, {0, 255, 0, 255} },
                { 350.0f, 150.0f, 0.8f, {0, 255, 0, 255} },
                { 250.0f, 350.0f, 0.8f, {0, 255, 0, 255} }
            };
            b1.indices = { 0, 1, 2 };
            b1.cullMode = FLYCAST_CULL_NONE;
            b1.depthFunc = FLYCAST_DEPTH_GREATER;
            data.batches.push_back(b1);

            DrawBatch b2; // Red (Far, drawn 2nd)
            b2.vertices = {
                { 100.0f, 100.0f, 0.2f, {255, 0, 0, 255} },
                { 300.0f, 100.0f, 0.2f, {255, 0, 0, 255} },
                { 200.0f, 300.0f, 0.2f, {255, 0, 0, 255} }
            };
            b2.indices = { 0, 1, 2 };
            b2.cullMode = FLYCAST_CULL_NONE;
            b2.depthFunc = FLYCAST_DEPTH_GREATER;
            data.batches.push_back(b2);
        }

        // --- 2. Culling Test (Winding) ---
        // BLUE Triangle: CCW (Area < 0) -> Front-face. Visible if Cull=Back.
        // YELLOW Triangle: CW (Area > 0) -> Back-face. Invisible if Cull=Back.
        {
            DrawBatch b3; // Blue (CCW)
            b3.vertices = {
                { 400.0f, 100.0f, 0.5f, {0, 0, 255, 255} },
                { 300.0f, 300.0f, 0.5f, {0, 0, 255, 255} }, // 2nd point on the left
                { 500.0f, 300.0f, 0.5f, {0, 0, 255, 255} }  // 3rd point on the right -> CCW
            };
            b3.indices = { 0, 1, 2 };
            b3.cullMode = FLYCAST_CULL_BACK; // Must remain visible
            data.batches.push_back(b3);

            DrawBatch b4; // Yellow (CW)
            b4.vertices = {
                { 400.0f, 200.0f, 0.5f, {255, 255, 0, 255} },
                { 500.0f, 400.0f, 0.5f, {255, 255, 0, 255} },
                { 300.0f, 400.0f, 0.5f, {255, 255, 0, 255} }  // CW
            };
            b4.indices = { 0, 1, 2 };
            b4.cullMode = FLYCAST_CULL_BACK; // Must be CULLED (Invisible)
            data.batches.push_back(b4);
        }
    }
};