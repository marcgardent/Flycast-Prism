#pragma once
#include "../TestCase.h"
#include "TestCommon.h"
#include <cmath>

class TestSPE02 : public TestCase {
public:
    std::string getId() const override { return "SPE-02"; }
    std::string getName() const override { return "Fog Corridor"; }
    std::string getDescription() const override { return "Fog rendering test using a lookup table (Table Fog). Simulates a deep corridor where colors fade into a gray fog based on depth."; }
    std::string getExpected() const override { return "A 3D corridor extending from Z=1.0 (Near, clear) to Z=0.05 (Far, fogged). The end of the corridor should be blended with gray fog."; }

    void prepare(TestData& data) override {
        // Build a corridor using 4 quads (floor, ceiling, left wall, right wall)
        // From z=1.0 (near) to z=0.05 (far)
        
        float x1 = 100, x2 = 1180;
        float y1 = 50,  y2 = 670;
        float zNear = 1.0f;
        float zFar = 0.05f;

        // Center point for perspective effect
        float cx = 640, cy = 360;
        
        // Inner rectangle (far end)
        float ix1 = cx - 50, ix2 = cx + 50;
        float iy1 = cy - 30, iy2 = cy + 30;

        auto addQuad = [&](float x1, float y1, float z1, float x2, float y2, float z2, 
                           float x3, float y3, float z3, float x4, float y4, float z4, 
                           uint8_t r, uint8_t g, uint8_t b) {
            DrawBatch batch;
            batch.fogMode = 0; // Table Fog
            batch.fogColor = 0xFF808080; // Gray fog
            batch.fogDensity = 128.0f;
            
            // Linear fog table for demonstration
            batch.fogTable.resize(128);
            for(int i=0; i<128; ++i) {
                // Fog increases as i decreases? 
                // Actually PVR Fog Table index is related to 1/W.
                // High 1/W (near) -> low index? 
                // Let's make it simple: 0.0 at index 127, 1.0 at index 0
                batch.fogTable[i] = (uint32_t)((127 - i) * 255 / 127); 
            }

            PluginVertex v1 = { x1, y1, z1, {r, g, b, 255}, {0, 0, 0, 0}, 0, 0 };
            PluginVertex v2 = { x2, y2, z2, {r, g, b, 255}, {0, 0, 0, 0}, 0, 0 };
            PluginVertex v3 = { x3, y3, z3, {r, g, b, 255}, {0, 0, 0, 0}, 0, 0 };
            PluginVertex v4 = { x4, y4, z4, {r, g, b, 255}, {0, 0, 0, 0}, 0, 0 };
            
            batch.vertices = { v1, v2, v3, v4 };
            batch.indices = { 0, 1, 2, 0, 2, 3 };
            data.batches.push_back(batch);
        };

        // Floor (Brownish)
        addQuad(x1, y2, zNear, x2, y2, zNear, ix2, iy2, zFar, ix1, iy2, zFar, 139, 69, 19);
        
        // Ceiling (Dark Blue)
        addQuad(x1, y1, zNear, ix1, iy1, zFar, ix2, iy1, zFar, x2, y1, zNear, 25, 25, 112);
        
        // Left Wall (Dark Green)
        addQuad(x1, y1, zNear, x1, y2, zNear, ix1, iy2, zFar, ix1, iy1, zFar, 0, 100, 0);
        
        // Right Wall (Dark Red)
        addQuad(x2, y1, zNear, ix2, iy1, zFar, ix2, iy2, zFar, x2, y2, zNear, 139, 0, 0);

        // Far wall (Black)
        addQuad(ix1, iy1, zFar, ix2, iy1, zFar, ix2, iy2, zFar, ix1, iy2, zFar, 0, 0, 0);
    }
};
