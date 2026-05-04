#pragma once
#include "benchmarker/TestCase.h"

class TestGEO06 : public TestCase {
public:
    std::string getId() const override { return "GEO-06"; }
    std::string getName() const override { return "Perspective-Correct Interpolation"; }
    std::string getDescription() const override { 
        return "Draws a large quad (2 triangles) tilted deeply into the Z-axis. "
               "Tests if the shader uses perspective-correct barycentric interpolation (1/W). "
               "If using affine screen-space interpolation (like PS1), the diagonal line separating the triangles will be extremely obvious and colors/textures will warp."; 
    }
    std::string getExpected() const override { 
        return "A smooth gradient across the entire quad. NO visible diagonal line cutting the quad in half."; 
    }

    void prepare(TestData& data) override {
        DrawBatch batch;
        batch.cullMode = FLYCAST_CULL_NONE;
        
        data.addStrip({
            { 100.0f, 400.0f, 0.9f, {255, 0, 0, 255} }, // BL
            { 540.0f, 400.0f, 0.9f, {0, 255, 0, 255} }, // BR
            { 250.0f, 100.0f, 0.1f, {0, 0, 255, 255} }, // TL
            { 390.0f, 100.0f, 0.1f, {255, 255, 0, 255} }  // TR
        }, batch);
    }
};