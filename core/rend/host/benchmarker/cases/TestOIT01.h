#pragma once
#include "TestCommon.h"

// ============================================================================
// OIT-01 : Order Independent Transparency
// Renders three overlapping translucent quads in FRONT-TO-BACK order.
// Without OIT (standard blending), the back quads would be occluded by depth
// or blend incorrectly. With OIT, they should blend correctly regardless of
// submission order.
// ============================================================================
class TestOIT01 : public TestCase {
public:
    std::string getId() const override { return "OIT-01"; }
    std::string getName() const override { return "Order Independent Transparency"; }
    std::string getDescription() const override { return "Order Independent Transparency (OIT) test. Three translucent quads (Red, Green, Blue) are submitted in front-to-back order."; }
    std::string getExpected() const override { return "Quads should blend correctly despite front-to-back submission. Z-order: Red (0.8, Front) > Green (0.5, Middle) > Blue (0.2, Back). All colors should mix properly in overlap areas."; }

    void prepare(TestData& data) override {
        // Quad colors: Red (Front), Green (Middle), Blue (Back)
        // In PVR, higher Z is closer to camera.
        
        auto addQuad = [&](float x, float y, float z, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
            DrawBatch batch;
            batch.vertices.resize(4);
            float size = 200.0f;
            
            // Top-left
            batch.vertices[0] = {};
            batch.vertices[0].x = x; batch.vertices[0].y = y; batch.vertices[0].z = z;
            batch.vertices[0].col[0] = r; batch.vertices[0].col[1] = g; batch.vertices[0].col[2] = b; batch.vertices[0].col[3] = a;

            // Top-right
            batch.vertices[1] = {};
            batch.vertices[1].x = x + size; batch.vertices[1].y = y; batch.vertices[1].z = z;
            batch.vertices[1].col[0] = r; batch.vertices[1].col[1] = g; batch.vertices[1].col[2] = b; batch.vertices[1].col[3] = a;

            // Bottom-right
            batch.vertices[2] = {};
            batch.vertices[2].x = x + size; batch.vertices[2].y = y + size; batch.vertices[2].z = z;
            batch.vertices[2].col[0] = r; batch.vertices[2].col[1] = g; batch.vertices[2].col[2] = b; batch.vertices[2].col[3] = a;

            // Bottom-left
            batch.vertices[3] = {};
            batch.vertices[3].x = x; batch.vertices[3].y = y + size; batch.vertices[3].z = z;
            batch.vertices[3].col[0] = r; batch.vertices[3].col[1] = g; batch.vertices[3].col[2] = b; batch.vertices[3].col[3] = a;

            batch.indices = { 0, 1, 2, 0, 2, 3 };
            
            batch.srcBlend = FLYCAST_BLEND_SRC_ALPHA;
            batch.dstBlend = FLYCAST_BLEND_INV_SRC_ALPHA;
            batch.depthWrite = false;
            batch.depthFunc = FLYCAST_DEPTH_ALWAYS; 
            batch.listType = FLYCAST_LIST_TRANSLUCENT;
            
            data.batches.push_back(std::move(batch));
        };

        // Render Front to Back (Submission order)
        // 1. Red (Front, Depth 0.8)
        // 2. Green (Middle, Depth 0.5)
        // 3. Blue (Back, Depth 0.2)
        addQuad(220.0f, 140.0f, 0.8f, 255, 0, 0, 128);   // Red
        addQuad(270.0f, 190.0f, 0.5f, 0, 255, 0, 128);   // Green
        addQuad(320.0f, 240.0f, 0.2f, 0, 0, 255, 128);   // Blue
    }
};
