#pragma once
#include "TestCommon.h"

// ============================================================================
// TRN-01 : Alpha Blending (Translucency)
// Renders a solid red background quad followed by a translucent blue 
// foreground quad (50% alpha).
// Blending: SrcAlpha / InvSrcAlpha
// Expected: A purple rectangle (50% red, 50% blue) in the center of a red screen.
// ============================================================================
class TestTRN01 : public TestCase {
public:
    std::string getId() const override { return "TRN-01"; }
    std::string getName() const override { return "Alpha Blending (Translucency)"; }
    std::string getDescription() const override { return "Alpha blending (translucency) test. Overlaps a translucent blue quad (50% alpha) on top of an opaque red background."; }
    std::string getExpected() const override { return "A purple rectangle in the center. The translucent blue quad (Z=0.6, Front) should correctly blend over the opaque red background (Z=0.5, Back)."; }

    void prepare(TestData& data) override {
        // 1. Background quad (Red, Opaque)
        {
            DrawBatch batch;
            batch.vertices.resize(4);
            
            // Top-left
            batch.vertices[0] = {};
            batch.vertices[0].x = 0.0f; batch.vertices[0].y = 0.0f; batch.vertices[0].z = 0.5f;
            batch.vertices[0].col[0] = 255; batch.vertices[0].col[1] = 0; batch.vertices[0].col[2] = 0; batch.vertices[0].col[3] = 255;

            // Top-right
            batch.vertices[1] = {};
            batch.vertices[1].x = 640.0f; batch.vertices[1].y = 0.0f; batch.vertices[1].z = 0.5f;
            batch.vertices[1].col[0] = 255; batch.vertices[1].col[1] = 0; batch.vertices[1].col[2] = 0; batch.vertices[1].col[3] = 255;

            // Bottom-right
            batch.vertices[2] = {};
            batch.vertices[2].x = 640.0f; batch.vertices[2].y = 480.0f; batch.vertices[2].z = 0.5f;
            batch.vertices[2].col[0] = 255; batch.vertices[2].col[1] = 0; batch.vertices[2].col[2] = 0; batch.vertices[2].col[3] = 255;

            // Bottom-left
            batch.vertices[3] = {};
            batch.vertices[3].x = 0.0f; batch.vertices[3].y = 480.0f; batch.vertices[3].z = 0.5f;
            batch.vertices[3].col[0] = 255; batch.vertices[3].col[1] = 0; batch.vertices[3].col[2] = 0; batch.vertices[3].col[3] = 255;

            batch.indices = { 0, 1, 2, 0, 2, 3 };
            
            batch.srcBlend = FLYCAST_BLEND_ONE;
            batch.dstBlend = FLYCAST_BLEND_ZERO;
            batch.depthWrite = true;
            batch.depthFunc = FLYCAST_DEPTH_ALWAYS; // Background quad
            
            data.batches.push_back(std::move(batch));
        }

        // 2. Foreground quad (Blue, 50% Alpha)
        {
            DrawBatch batch;
            batch.vertices.resize(4);
            
            // Top-left
            batch.vertices[0] = {};
            batch.vertices[0].x = 160.0f; batch.vertices[0].y = 120.0f; batch.vertices[0].z = 0.6f;
            batch.vertices[0].col[0] = 0; batch.vertices[0].col[1] = 0; batch.vertices[0].col[2] = 255; batch.vertices[0].col[3] = 128;

            // Top-right
            batch.vertices[1] = {};
            batch.vertices[1].x = 480.0f; batch.vertices[1].y = 120.0f; batch.vertices[1].z = 0.6f;
            batch.vertices[1].col[0] = 0; batch.vertices[1].col[1] = 0; batch.vertices[1].col[2] = 255; batch.vertices[1].col[3] = 128;

            // Bottom-right
            batch.vertices[2] = {};
            batch.vertices[2].x = 480.0f; batch.vertices[2].y = 360.0f; batch.vertices[2].z = 0.6f;
            batch.vertices[2].col[0] = 0; batch.vertices[2].col[1] = 0; batch.vertices[2].col[2] = 255; batch.vertices[2].col[3] = 128;

            // Bottom-left
            batch.vertices[3] = {};
            batch.vertices[3].x = 160.0f; batch.vertices[3].y = 360.0f; batch.vertices[3].z = 0.6f;
            batch.vertices[3].col[0] = 0; batch.vertices[3].col[1] = 0; batch.vertices[3].col[2] = 255; batch.vertices[3].col[3] = 128;

            batch.indices = { 0, 1, 2, 0, 2, 3 };

            batch.srcBlend = FLYCAST_BLEND_SRC_ALPHA;
            batch.dstBlend = FLYCAST_BLEND_INV_SRC_ALPHA;
            batch.depthWrite = false;
            batch.depthFunc = FLYCAST_DEPTH_GEQUAL; // Should pass over 0.5 background
            
            data.batches.push_back(std::move(batch));
        }
    }
};
