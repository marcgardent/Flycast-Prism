#pragma once
#include "TestCase.h"

class TestGEO01 : public TestCase {
public:
    std::string getId() const override { return "GEO-01"; }
    std::string getName() const override { return "Simple Square"; }
    std::string getDescription() const override { return "A simple opaque square (2 triangles)."; }

    void prepare(TestData& data) override {
        data.vertices.resize(4);
        float centerX = 320.0f;
        float centerY = 240.0f;
        float size = 100.0f;

        data.vertices[0] = { centerX - size, centerY - size, 0.5f, {255, 255, 255, 255} };
        data.vertices[1] = { centerX + size, centerY - size, 0.5f, {255, 255, 255, 255} };
        data.vertices[2] = { centerX + size, centerY + size, 0.5f, {255, 255, 255, 255} };
        data.vertices[3] = { centerX - size, centerY + size, 0.5f, {255, 255, 255, 255} };

        data.indices = { 0, 1, 2, 0, 2, 3 };
    }
};

class TestGEO03 : public TestCase {
public:
    std::string getId() const override { return "GEO-03"; }
    std::string getName() const override { return "Clipping (Scissor)"; }
    std::string getDescription() const override { return "A full-screen polygon with a 320x240 Scissor zone in the center."; }

    void prepare(TestData& data) override {
        data.vertices.resize(4);
        // Full-screen white quad (640x480 space)
        data.vertices[0] = { 0.0f,   0.0f,   0.5f, {255, 255, 255, 255} };
        data.vertices[1] = { 640.0f, 0.0f,   0.5f, {255, 255, 255, 255} };
        data.vertices[2] = { 640.0f, 480.0f, 0.5f, {255, 255, 255, 255} };
        data.vertices[3] = { 0.0f,   480.0f, 0.5f, {255, 255, 255, 255} };

        data.indices = { 0, 1, 2, 0, 2, 3 };

        // 320x240 scissor in the center
        data.scissorEnable = true;
        data.scissorX = 160;
        data.scissorY = 120;
        data.scissorW = 320;
        data.scissorH = 240;
    }
};

class TestSHD01 : public TestCase {
public:
    std::string getId() const override { return "SHD-01"; }
    std::string getName() const override { return "Interpolation (Gouraud)"; }
    std::string getDescription() const override { return "A triangle with Red/Green/Blue vertices."; }

    void prepare(TestData& data) override {
        data.vertices.resize(3);
        data.vertices[0] = { 320.0f, 100.0f, 0.5f, {255, 0, 0, 255} };
        data.vertices[1] = { 500.0f, 400.0f, 0.5f, {0, 255, 0, 255} };
        data.vertices[2] = { 140.0f, 400.0f, 0.5f, {0, 0, 255, 255} };

        data.indices = { 0, 1, 2 };
    }
};
