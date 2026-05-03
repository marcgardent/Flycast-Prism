#pragma once
#include "TestCommon.h"

// ============================================================================
// GC-01 : Garbage Collection Test
// Renders a texture for a few frames, then stops.
// After 120 frames of disuse, the Host (benchmarker) should call destroy_texture.
// ============================================================================
class TestGC01 : public TestCase {
    uint32_t frame = 0;
public:
    std::string getId() const override { return "GC-01"; }
    std::string getName() const override { return "Garbage Collection"; }
    std::string getDescription() const override { return "Garbage collection (GC) test. Verifies that GPU resources (textures) are automatically destroyed after a period of inactivity (120 frames)."; }
    std::string getExpected() const override { return "A white textured triangle (Z=0.5) visible for 10 frames, then replaced by a tiny red dot. Check logs for 'Destroying texture' around frame 130."; }
    uint32_t getFrameCount() const override { return 250; }

    void update(float dt) override {
        frame++;
    }


    void prepare(TestData& data) override {
        // Only render the texture during the first 10 frames
        if (frame < 10) {
            DrawBatch batch;
            batch.vertices.resize(3);
            batch.vertices[0] = { 0, 0, 0.5f, {255, 255, 255, 255}, {0,0,0,0}, 0, 0 };
            batch.vertices[1] = { 100, 0, 0.5f, {255, 255, 255, 255}, {0,0,0,0}, 1, 0 };
            batch.vertices[2] = { 0, 100, 0.5f, {255, 255, 255, 255}, {0,0,0,0}, 0, 1 };
            batch.indices = { 0, 1, 2 };

            batch.texMode = FLYCAST_TEX_PAL8;
            batch.texWidth = 8;
            batch.texHeight = 8;
            batch.texData.assign(64, 1); // 8x8 texture filled with index 1
            
            data.batches.push_back(std::move(batch));
        } else {
            // After frame 10, we submit an empty batch or a batch without texture
            DrawBatch batch;
            batch.vertices.resize(3);
            batch.vertices[0] = { 0, 0, 0.5f, {255, 0, 0, 255}, {0,0,0,0}, 0, 0 }; // Red triangle
            batch.vertices[1] = { 10, 0, 0.5f, {255, 0, 0, 255}, {0,0,0,0}, 0, 0 };
            batch.vertices[2] = { 0, 10, 0.5f, {255, 0, 0, 255}, {0,0,0,0}, 0, 0 };
            batch.indices = { 0, 1, 2 };
            data.batches.push_back(std::move(batch));
        }
    }
};
