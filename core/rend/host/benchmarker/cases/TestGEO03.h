#pragma once
#include "benchmarker/TestCase.h"

class TestGEO03 : public TestCase {
public:
    std::string getId() const override { return "GEO-03"; }
    std::string getName() const override { return "Clipping (Scissor)"; }
    std::string getDescription() const override { return "Scissor rectangle test. Clips rendering to a specific 2D rectangular region."; }
    std::string getExpected() const override { return "A white rectangle (Z=0.5) in the center (320x240), clipping a full-screen quad."; }

    void prepare(TestData& data) override {
        DrawBatch batch;
        batch.scissorEnable = true;
        batch.scissorX = 160;
        batch.scissorY = 120;
        batch.scissorW = 320;
        batch.scissorH = 240;

        data.addStrip({
            { 0.0f,   0.0f,   0.5f, {255, 255, 255, 255} }, // TL
            { 640.0f, 0.0f,   0.5f, {255, 255, 255, 255} }, // TR
            { 0.0f,   480.0f, 0.5f, {255, 255, 255, 255} }, // BL
            { 640.0f, 480.0f, 0.5f, {255, 255, 255, 255} }  // BR
        }, batch);
    }
};