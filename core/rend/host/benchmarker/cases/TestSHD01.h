#pragma once
#include "benchmarker/TestCase.h"

class TestSHD01 : public TestCase {
public:
    std::string getId() const override { return "SHD-01"; }
    std::string getName() const override { return "Interpolation (Gouraud)"; }
    std::string getDescription() const override { return "A triangle with Red/Green/Blue vertices."; }

    void prepare(TestData& data) override {
        DrawBatch batch;
        batch.vertices.resize(3);
        batch.vertices[0] = { 320.0f, 100.0f, 0.5f, {255, 0, 0, 255} };
        batch.vertices[1] = { 500.0f, 400.0f, 0.5f, {0, 255, 0, 255} };
        batch.vertices[2] = { 140.0f, 400.0f, 0.5f, {0, 0, 255, 255} };

        batch.indices = { 0, 1, 2 };
        data.batches.push_back(batch);
    }
};
