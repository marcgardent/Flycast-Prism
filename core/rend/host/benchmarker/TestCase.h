#pragma once
#include <string>
#include <vector>
#include "../flycast_plugin_api.h"

struct TestData {
    std::vector<PluginVertex> vertices;
    std::vector<uint32_t> indices;
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
