#pragma once
#include "benchmarker/TestCase.h"


class TestGEO01 : public TestCase {
public:
    std::string getId() const override { return "GEO-01"; }
    std::string getName() const override { return "Simple Square"; }
    std::string getDescription() const override { return "Basic geometry rendering: a simple opaque square made of two triangles."; }
    std::string getExpected() const override { return "A solid white square (Z=0.5) centered on the screen on a dark background."; }

    void prepare(TestData& data) override {
        DrawBatch batch;
        batch.vertices.resize(4);
        float centerX = 320.0f;
        float centerY = 240.0f;
        float size = 100.0f;

        batch.vertices[0] = { centerX - size, centerY - size, 0.5f, {255, 255, 255, 255} };
        batch.vertices[1] = { centerX + size, centerY - size, 0.5f, {255, 255, 255, 255} };
        batch.vertices[2] = { centerX + size, centerY + size, 0.5f, {255, 255, 255, 255} };
        batch.vertices[3] = { centerX - size, centerY + size, 0.5f, {255, 255, 255, 255} };

        batch.indices = { 0, 1, 2, 0, 2, 3 };
        data.batches.push_back(batch);
    }
};
