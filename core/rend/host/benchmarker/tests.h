#pragma once
#include "TestCase.h"

class TestGEO01 : public TestCase {
public:
    std::string getId() const override { return "GEO-01"; }
    std::string getName() const override { return "Simple Square"; }
    std::string getDescription() const override { return "A simple opaque square (2 triangles)."; }

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

class TestGEO02 : public TestCase {
public:
    std::string getId() const override { return "GEO-02"; }
    std::string getName() const override { return "Culling"; }
    std::string getDescription() const override { return "Three overlapping triangles with different winding and culling."; }

    void prepare(TestData& data) override {
        // 1. Red Triangle: CCW, Cull None (Visible)
        {
            DrawBatch batch;
            batch.vertices.resize(3);
            batch.vertices[0] = { 320.0f, 100.0f, 0.5f, {255, 0, 0, 255} };
            batch.vertices[1] = { 420.0f, 300.0f, 0.5f, {255, 0, 0, 255} };
            batch.vertices[2] = { 220.0f, 300.0f, 0.5f, {255, 0, 0, 255} };
            batch.indices = { 0, 2, 1 }; // CCW
            batch.cullMode = FLYCAST_CULL_NONE;
            data.batches.push_back(batch);
        }

        // 2. Green Triangle: CCW, Cull Back (Visible if CCW=Front)
        {
            DrawBatch batch;
            batch.vertices.resize(3);
            batch.vertices[0] = { 320.0f, 150.0f, 0.4f, {0, 255, 0, 255} };
            batch.vertices[1] = { 420.0f, 350.0f, 0.4f, {0, 255, 0, 255} };
            batch.vertices[2] = { 220.0f, 350.0f, 0.4f, {0, 255, 0, 255} };
            batch.indices = { 0, 2, 1 }; // CCW
            batch.cullMode = FLYCAST_CULL_BACK;
            data.batches.push_back(batch);
        }

        // 3. Blue Triangle: CW, Cull Back (Hidden if CCW=Front)
        {
            DrawBatch batch;
            batch.vertices.resize(3);
            batch.vertices[0] = { 320.0f, 200.0f, 0.3f, {0, 0, 255, 255} };
            batch.vertices[1] = { 420.0f, 400.0f, 0.3f, {0, 0, 255, 255} };
            batch.vertices[2] = { 220.0f, 400.0f, 0.3f, {0, 0, 255, 255} };
            batch.indices = { 0, 1, 2 }; // CW
            batch.cullMode = FLYCAST_CULL_BACK;
            data.batches.push_back(batch);
        }
    }
};

class TestGEO03 : public TestCase {
public:
    std::string getId() const override { return "GEO-03"; }
    std::string getName() const override { return "Clipping (Scissor)"; }
    std::string getDescription() const override { return "A full-screen polygon with a 320x240 Scissor zone in the center."; }

    void prepare(TestData& data) override {
        DrawBatch batch;
        batch.vertices.resize(4);
        batch.vertices[0] = { 0.0f,   0.0f,   0.5f, {255, 255, 255, 255} };
        batch.vertices[1] = { 640.0f, 0.0f,   0.5f, {255, 255, 255, 255} };
        batch.vertices[2] = { 640.0f, 480.0f, 0.5f, {255, 255, 255, 255} };
        batch.vertices[3] = { 0.0f,   480.0f, 0.5f, {255, 255, 255, 255} };

        batch.indices = { 0, 1, 2, 0, 2, 3 };
        batch.scissorEnable = true;
        batch.scissorX = 160;
        batch.scissorY = 120;
        batch.scissorW = 320;
        batch.scissorH = 240;
        data.batches.push_back(batch);
    }
};

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
