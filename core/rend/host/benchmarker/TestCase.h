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
};
