#pragma once
#include "benchmarker/TestCase.h"


class TestGEO01 : public TestCase {
public:
    std::string getId() const override { return "GEO-01"; }
    std::string getName() const override { return "Simple Square"; }
    std::string getDescription() const override { return "Basic geometry rendering: a simple opaque square made of two triangles."; }
    std::string getExpected() const override { return "A solid white square (Z=0.5) centered on the screen on a dark background."; }

    void prepare(TestData& data) override {
        float centerX = 320.0f;
        float centerY = 240.0f;
        float size = 100.0f;

        data.addStrip({
            { .x = centerX - size, .y = centerY - size, .z_inv = 0.5f, .col = {255, 255, 255, 255} }, // TL
            { .x = centerX + size, .y = centerY - size, .z_inv = 0.5f, .col = {255, 255, 255, 255} }, // TR
            { .x = centerX - size, .y = centerY + size, .z_inv = 0.5f, .col = {255, 255, 255, 255} }, // BL
            { .x = centerX + size, .y = centerY + size, .z_inv = 0.5f, .col = {255, 255, 255, 255} }  // BR
        });
    }
};
