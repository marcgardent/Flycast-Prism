#pragma once
#include "benchmarker/TestCase.h"

class TestSHD02 : public TestCase {
public:
    std::string getId() const override { return "SHD-02"; }
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
            {  95.0f,  450.0f, 0.9f, {255, 0, 0, 255} },   // BL (Red)
            { 545.0f,  450.0f, 0.9f, {0, 255, 0, 255} },   // BR (Green)
            { 295.0f,  150.0f, 0.1f, {0, 0, 255, 255} },   // TL (Blue)
            { 345.0f,  150.0f, 0.1f, {255, 255, 0, 255} }  // TR (Yellow)
        }, batch);
    }
};