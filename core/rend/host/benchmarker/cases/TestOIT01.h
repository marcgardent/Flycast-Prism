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
        auto addQuad = [&](float x, float y, float z, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
            DrawBatch batch;
            batch.srcBlend = FLYCAST_BLEND_SRC_ALPHA;
            batch.dstBlend = FLYCAST_BLEND_INV_SRC_ALPHA;
            batch.depthWrite = false;
            batch.depthFunc = FLYCAST_DEPTH_ALWAYS; 
            batch.listType = FLYCAST_LIST_TRANSLUCENT;

            float size = 200.0f;
            data.addStrip({
                { x, y, z, {r, g, b, a} }, // TL
                { x + size, y, z, {r, g, b, a} }, // TR
                { x, y + size, z, {r, g, b, a} }, // BL
                { x + size, y + size, z, {r, g, b, a} }  // BR
            }, batch);
        };

        // Render Front to Back (Submission order)
        addQuad(220.0f, 140.0f, 0.8f, 255, 0, 0, 128);   // Red
        addQuad(270.0f, 190.0f, 0.5f, 0, 255, 0, 128);   // Green
        addQuad(320.0f, 240.0f, 0.2f, 0, 0, 255, 128);   // Blue
    }
};
