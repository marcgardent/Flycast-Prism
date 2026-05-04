#pragma once
#include "benchmarker/TestCase.h"

class TestSHD01 : public TestCase {
public:
    std::string getId() const override { return "SHD-01"; }
    std::string getName() const override { return "Interpolation (Gouraud)"; }
    std::string getDescription() const override { return "Vertex color interpolation (Gouraud shading) test. A single triangle with different colors at each vertex."; }
    std::string getExpected() const override { return "A large triangle (Z=0.5) with a smooth color gradient (Red-Green-Blue)."; }

    void prepare(TestData& data) override {
        data.addStrip({
            { 320.0f, 100.0f, 0.5f, {255, 0, 0, 255} },
            { 500.0f, 400.0f, 0.5f, {0, 255, 0, 255} },
            { 140.0f, 400.0f, 0.5f, {0, 0, 255, 255} }
        });
    }
};
