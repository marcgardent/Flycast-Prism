#pragma once
#include <string>
#include <vector>
#include <cstring>
#include "../flycast_plugin_api.h"

// Structure simplifiée pour écrire les tests plus rapidement
struct TestVertex {
    float x, y, z_inv;
    uint8_t col[4] = {255, 255, 255, 255};
    uint8_t spc[4] = {0, 0, 0, 0};
    float u = 0.0f;
    float v = 0.0f;
};

struct DrawBatch {
    uint32_t vertexOffset = 0;
    uint32_t vertexCount = 0;

    bool scissorEnable = false;
    int32_t scissorX = 0;
    int32_t scissorY = 0;
    int32_t scissorW = 0;
    int32_t scissorH = 0;

    FlycastCullMode cullMode = FLYCAST_CULL_NONE;

    // Texture (TEX-01)
    FlycastTexMode texMode = FLYCAST_TEX_NONE;
    uint32_t texWidth = 0;
    uint32_t texHeight = 0;
    std::vector<uint8_t> texData;      // texWidth * texHeight bytes (8BPP indices)
    std::vector<uint32_t> palette;     // 256 RGBA8 entries (R, G, B, A in memory)

    // Transparency (TRN-01)
    FlycastBlendFactor srcBlend = FLYCAST_BLEND_ONE;
    FlycastBlendFactor dstBlend = FLYCAST_BLEND_ZERO;
    FlycastDepthFunc depthFunc = FLYCAST_DEPTH_GEQUAL;
    bool depthWrite = true;

    // Specular (SPE-01)
    bool offsetEnable = false;

    // Fog (SPE-02)
    uint32_t fogMode = 2; // NONE
    uint32_t fogColor = 0xFFA0A0A0; // Default gray fog
    uint32_t fogVertexColor = 0xFFFFFFFF;
    float fogDensity = 1.0f;
    uint32_t fogClampMin = 0x00000000;
    uint32_t fogClampMax = 0xFFFFFFFF;
    std::vector<uint32_t> fogTable; // 128 entries

    // OIT (OIT-01)
    FlycastListType listType = FLYCAST_LIST_OPAQUE;
};

struct TestData {
    std::vector<PluginVertex> vertices;
    std::vector<DrawBatch> batches;

    // Nouvelle version qui accepte les TestVertex et fait la conversion hardware
    void addStrip(const std::vector<TestVertex>& stripVerts, const DrawBatch& state = {}) {
        DrawBatch b = state;
        b.vertexOffset = (uint32_t)vertices.size();
        b.vertexCount = (uint32_t)stripVerts.size();

        for (const auto& v : stripVerts) {
            PluginVertex pv;
            pv.x = v.x;
            pv.y = v.y;
            pv.z = v.z_inv;
            std::memcpy(pv.col, v.col, 4);
            std::memcpy(pv.spc, v.spc, 4);

            // LA CORRECTION EST ICI : Pré-multiplication des UVs par 1/W (z_inv)
            // comme l'exige l'architecture matérielle PVR émulée
            pv.u = v.u * v.z_inv;
            pv.v = v.v * v.z_inv;

            vertices.push_back(pv);
        }

        batches.push_back(b);
    }
};

class TestCase {
public:
    virtual ~TestCase() = default;
    virtual std::string getId() const = 0;
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;
    virtual std::string getExpected() const = 0;

    virtual void prepare(TestData& data) = 0;
    virtual void update(float dt) {}
    virtual uint32_t getFrameCount() const { return 1; }
    virtual uint32_t getTargetFPS() const { return 60; }
};