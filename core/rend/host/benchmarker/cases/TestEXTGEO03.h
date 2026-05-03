#pragma once

class TestEXTGEO03 : public TestCase {
public:
    std::string getId() const override { return "EXT-GEO-03"; }
    std::string getName() const override { return "Multi-Batch State Leak (Tile Counters)"; }
    std::string getDescription() const override { 
        return "Draws multiple distinct batches in the same area. "
               "Validates that the renderer correctly clears 'tile_counters' between process() calls. "
               "If not cleared, Batch 2 reads out-of-bounds indices from Batch 1, causing the 'spider web' bug."; 
    }
    std::string getExpected() const override { 
        return "A large red background with a smaller green triangle strictly in the center. No geometric corruption."; 
    }
    uint32_t getFrameCount() const override { return 10; }

    void prepare(TestData& data) override {
        // Batch 1: Large Red Triangles (fills many tiles)
        // This will increment the compute shader's tile_counters significantly.
        {
            DrawBatch batch;
            batch.vertices.resize(4);
            batch.vertices[0] = { 0.0f, 0.0f, 0.8f, {255, 0, 0, 255} };
            batch.vertices[1] = { 640.0f, 0.0f, 0.8f, {255, 0, 0, 255} };
            batch.vertices[2] = { 0.0f, 480.0f, 0.8f, {255, 0, 0, 255} };
            batch.vertices[3] = { 640.0f, 480.0f, 0.8f, {255, 0, 0, 255} };
            batch.indices = { 0, 1, 2, 2, 1, 3 }; // 2 Triangles
            batch.cullMode = FLYCAST_CULL_NONE;
            data.batches.push_back(batch);
        }

        // Batch 2: Small Green Triangle (in the same tiles)
        // If tile_counters wasn't cleared, this batch starts appending at count > 0.
        // The rasterizer will loop over the old tri_idx from Batch 1, 
        // causing out-of-bounds reads in Batch 2's smaller index buffer -> Spider Web !
        {
            DrawBatch batch;
            batch.vertices.resize(3);
            batch.vertices[0] = { 300.0f, 200.0f, 0.5f, {0, 255, 0, 255} };
            batch.vertices[1] = { 340.0f, 200.0f, 0.5f, {0, 255, 0, 255} };
            batch.vertices[2] = { 320.0f, 240.0f, 0.5f, {0, 255, 0, 255} };
            batch.indices = { 0, 1, 2 }; // 1 Triangle
            batch.cullMode = FLYCAST_CULL_NONE;
            data.batches.push_back(batch);
        }
    }
};