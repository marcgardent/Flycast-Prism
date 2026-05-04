#pragma once
#include "../TestCase.h"
#include "TestCommon.h"

class TestSPE01 : public TestCase {
public:
    std::string getId() const override { return "SPE-01"; }
    std::string getName() const override { return "Specular (Offset Color)"; }
    std::string getDescription() const override { return "Specular (Offset Color) test. Validates the additive blending of the secondary color (Offset Color) onto the primary vertex color."; }
    std::string getExpected() const override { return "Three triangles (all Z=0.5): Blue (Black+Blue offset), Yellow (Green+Red offset), and Green (No offset)."; }

    void prepare(TestData& data) override {
        // 1. Triangle GAUCHE : Noir + Offset Bleu = BLEU
        {
            DrawBatch b;
            b.offsetEnable = true;
            data.addStrip({
                { 200, 100, 0.5f, {0, 0, 0, 255}, {0, 0, 255, 255}, 0, 0 },
                { 350, 400, 0.5f, {0, 0, 0, 255}, {0, 0, 255, 255}, 0, 0 },
                {  50, 400, 0.5f, {0, 0, 0, 255}, {0, 0, 255, 255}, 0, 0 }
            }, b);
        }

        // 2. Triangle MILIEU : Vert + Offset Rouge = JAUNE
        {
            DrawBatch b;
            b.offsetEnable = true;
            data.addStrip({
                { 640, 100, 0.5f, {0, 255, 0, 255}, {255, 0, 0, 255}, 0, 0 },
                { 790, 400, 0.5f, {0, 255, 0, 255}, {255, 0, 0, 255}, 0, 0 },
                { 490, 400, 0.5f, {0, 255, 0, 255}, {255, 0, 0, 255}, 0, 0 }
            }, b);
        }

        // 3. Triangle DROITE : Vert simple (Offset désactivé) = VERT
        {
            DrawBatch b;
            b.offsetEnable = false;
            data.addStrip({
                { 1080, 100, 0.5f, {0, 255, 0, 255}, {255, 255, 255, 255}, 0, 0 },
                { 1230, 400, 0.5f, {0, 255, 0, 255}, {255, 255, 255, 255}, 0, 0 },
                {  930, 400, 0.5f, {0, 255, 0, 255}, {255, 255, 255, 255}, 0, 0 }
            }, b);
        }
    }
};
