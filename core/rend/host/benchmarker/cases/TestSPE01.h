#pragma once
#include "../TestCase.h"
#include "TestCommon.h"

class TestSPE01 : public TestCase {
public:
    std::string getId() const override { return "SPE-01"; }
    std::string getName() const override { return "Specular (Offset Color)"; }
    std::string getDescription() const override { 
        return "Verify additive Offset Color: Left=Blue (Black+Blue), Middle=Yellow (Green+Red), Right=Green (No Offset)."; 
    }

    void prepare(TestData& data) override {
        // 1. Triangle GAUCHE : Noir + Offset Bleu = BLEU
        // Prouve que l'offset est ajouté même si la base est noire
        {
            DrawBatch batch;
            batch.offsetEnable = true;
            PluginVertex v1 = { 200, 100, 0.5f, {0, 0, 0, 255}, {0, 0, 255, 255}, 0, 0 };
            PluginVertex v2 = { 350, 400, 0.5f, {0, 0, 0, 255}, {0, 0, 255, 255}, 0, 0 };
            PluginVertex v3 = {  50, 400, 0.5f, {0, 0, 0, 255}, {0, 0, 255, 255}, 0, 0 };
            batch.vertices = { v1, v2, v3 };
            batch.indices = { 0, 1, 2 };
            data.batches.push_back(batch);
        }

        // 2. Triangle MILIEU : Vert + Offset Rouge = JAUNE
        // Prouve le mélange additif (0,255,0) + (255,0,0) = (255,255,0)
        {
            DrawBatch batch;
            batch.offsetEnable = true;
            PluginVertex v1 = { 640, 100, 0.5f, {0, 255, 0, 255}, {255, 0, 0, 255}, 0, 0 };
            PluginVertex v2 = { 790, 400, 0.5f, {0, 255, 0, 255}, {255, 0, 0, 255}, 0, 0 };
            PluginVertex v3 = { 490, 400, 0.5f, {0, 255, 0, 255}, {255, 0, 0, 255}, 0, 0 };
            batch.vertices = { v1, v2, v3 };
            batch.indices = { 0, 1, 2 };
            data.batches.push_back(batch);
        }

        // 3. Triangle DROITE : Vert simple (Offset désactivé) = VERT
        // Sert de témoin
        {
            DrawBatch batch;
            batch.offsetEnable = false;
            PluginVertex v1 = { 1080, 100, 0.5f, {0, 255, 0, 255}, {255, 255, 255, 255}, 0, 0 };
            PluginVertex v2 = { 1230, 400, 0.5f, {0, 255, 0, 255}, {255, 255, 255, 255}, 0, 0 };
            PluginVertex v3 = {  930, 400, 0.5f, {0, 255, 0, 255}, {255, 255, 255, 255}, 0, 0 };
            batch.vertices = { v1, v2, v3 };
            batch.indices = { 0, 1, 2 };
            data.batches.push_back(batch);
        }
    }
};
