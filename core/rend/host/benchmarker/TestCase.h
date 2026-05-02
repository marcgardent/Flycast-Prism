#pragma once
#include <string>
#include <vector>
#include "../flycast_plugin_api.h"

struct DrawBatch {
    std::vector<PluginVertex> vertices;
    std::vector<uint32_t> indices;

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
    std::vector<uint32_t> palette;     // 256 ARGB32 entries

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
    std::vector<DrawBatch> batches;
};

class TestCase {
public:
    virtual ~TestCase() = default;
    virtual std::string getId() const = 0;
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;
    
    virtual void prepare(TestData& data) = 0;
    virtual void update(float dt) {}
    virtual uint32_t getFrameCount() const { return 1; }
};
