#pragma once
#include "benchmarker/TestCase.h"

class TestGEO04 : public TestCase {
public:
    std::string getId() const override { return "GEO-04"; }
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
        // Batch 1: Large Red Quad
        DrawBatch b1;
        b1.cullMode = FLYCAST_CULL_NONE;
        data.addStrip({
            { .x = 0.0f, .y = 0.0f, .z_inv = 0.2f, .col = {255, 0, 0, 255} }, // TL
            { .x = 640.0f, .y = 0.0f, .z_inv = 0.2f, .col = {255, 0, 0, 255} }, // TR
            { .x = 0.0f, .y = 480.0f, .z_inv = 0.2f, .col = {255, 0, 0, 255} }, // BL
            { .x = 640.0f, .y = 480.0f, .z_inv = 0.2f, .col = {255, 0, 0, 255} }  // BR
        }, b1);

        // Batch 2: Small Green Triangle
        DrawBatch b2;
        b2.cullMode = FLYCAST_CULL_NONE;
        data.addStrip({
            { .x = 300.0f, .y = 200.0f, .z_inv = 0.5f, .col = {0, 255, 0, 255} },
            { .x = 340.0f, .y = 200.0f, .z_inv = 0.5f, .col = {0, 255, 0, 255} },
            { .x = 320.0f, .y = 240.0f, .z_inv = 0.5f, .col = {0, 255, 0, 255} }
        }, b2);
    }
};