#pragma once
#include "TestCommon.h"
#include <cstring>

extern uint32_t palette32_ram[1024];

class TestTEX01 : public TestCase {
public:
    std::string getId() const override { return "TEX-01"; }
    std::string getName() const override { return "Palette Lookup (8BPP)"; }
    std::string getDescription() const override { return "Texture mapping with palette lookup (8BPP). Each texel is an index into a 256-color palette (within a 1024 hardware bank)."; }
    std::string getExpected() const override { return "A full-screen rainbow gradient (Z=0.5) using an 8BPP indexed texture."; }

    void prepare(TestData& data) override {
        constexpr uint32_t TEX_W = 256;
        constexpr uint32_t TEX_H = 256;

        std::vector<uint32_t> pal(1024, 0);
        for (int i = 0; i < 256; ++i) {
            float hue = (float)i / 256.0f * 360.0f;
            pal[i] = hsl_to_rgba32(hue, 1.0f, 0.5f);
        }
        memcpy(palette32_ram, pal.data(), 1024 * sizeof(uint32_t));

        std::vector<uint8_t> tex(TEX_W * TEX_H);
        for (uint32_t y = 0; y < TEX_H; ++y) {
            for (uint32_t x = 0; x < TEX_W; ++x) {
                tex[y * TEX_W + x] = (uint8_t)((x + y) & 0xFF);
            }
        }

        DrawBatch batch;
        batch.texMode   = FLYCAST_TEX_PAL8;
        batch.texWidth  = TEX_W;
        batch.texHeight = TEX_H;
        batch.texData   = std::move(tex);
        batch.palette   = std::move(pal);

        data.addStrip({
            { 0.0f,   0.0f,   0.5f, {255, 255, 255, 255}, {0,0,0,0}, 0.0f, 0.0f }, // TL
            { 640.0f, 0.0f,   0.5f, {255, 255, 255, 255}, {0,0,0,0}, 1.0f, 0.0f }, // TR
            { 0.0f,   480.0f, 0.5f, {255, 255, 255, 255}, {0,0,0,0}, 0.0f, 1.0f }, // BL
            { 640.0f, 480.0f, 0.5f, {255, 255, 255, 255}, {0,0,0,0}, 1.0f, 1.0f }  // BR
        }, batch);
    }
};