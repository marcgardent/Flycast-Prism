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
            batch.srcBlend = FLYCAST_BLEND_ONE;
            batch.dstBlend = FLYCAST_BLEND_ZERO;
            batch.depthWrite = true;
            batch.depthFunc = FLYCAST_DEPTH_ALWAYS; 
            
            data.addStrip({
                { 0.0f, 0.0f, 0.5f, {255, 0, 0, 255} }, // TL
                { 640.0f, 0.0f, 0.5f, {255, 0, 0, 255} }, // TR
                { 0.0f, 480.0f, 0.5f, {255, 0, 0, 255} }, // BL
                { 640.0f, 480.0f, 0.5f, {255, 0, 0, 255} }  // BR
            }, batch);
        }

        // 2. Foreground quad (Blue, 50% Alpha)
        {
            DrawBatch batch;
            batch.srcBlend = FLYCAST_BLEND_SRC_ALPHA;
            batch.dstBlend = FLYCAST_BLEND_INV_SRC_ALPHA;
            batch.depthWrite = false;
            batch.depthFunc = FLYCAST_DEPTH_GEQUAL;
            
            data.addStrip({
                { 160.0f, 120.0f, 0.6f, {0, 0, 255, 128} }, // TL
                { 480.0f, 120.0f, 0.6f, {0, 0, 255, 128} }, // TR
                { 160.0f, 360.0f, 0.6f, {0, 0, 255, 128} }, // BL
                { 480.0f, 360.0f, 0.6f, {0, 0, 255, 128} }  // BR
            }, batch);
        }
    }
};
