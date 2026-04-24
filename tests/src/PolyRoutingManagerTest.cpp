#include <gtest/gtest.h>
#include "../../core/rend/vulkan/gbuffer/PolyRoutingManager.h"
#include <fstream>

using namespace rend;

class PolyRoutingManagerTest : public ::testing::Test {
protected:
    void CreateJson(const std::string& json) {
        std::ofstream ofs("test_routing.json");
        ofs << json;
        ofs.close();
        manager.LoadConfig("test_routing.json");
    }

    PolyRoutingManager manager;
};

TEST_F(PolyRoutingManagerTest, BasicStencils) {
    CreateJson(R"raw({
"stencils": {
    "HUD": "texture_hash == 11259375",
    "SKY": "wp_z > 1000",
    "SCENE": "1"
}
})raw");

    PolyData data = {};
    data.texture_hash = 0xABCDEF; // 11259375
    EXPECT_TRUE(manager.IsHud(data));
    EXPECT_FALSE(manager.IsSky(data));

    data = {};
    data.wp_z = 1001;
    EXPECT_FALSE(manager.IsHud(data));
    EXPECT_TRUE(manager.IsSky(data));
    EXPECT_TRUE(manager.IsScene(data));
}

TEST_F(PolyRoutingManagerTest, IsCloseFunction) {
    CreateJson(R"raw({
"stencils": {
    "HUD": "isClose(wp_x, 10.0, abs_tol=0.01)"
}
})raw");

    PolyData data = {};
    data.wp_x = 10.005;
    EXPECT_TRUE(manager.IsHud(data));

    data.wp_x = 10.05;
    EXPECT_FALSE(manager.IsHud(data));
}

TEST_F(PolyRoutingManagerTest, MalformedExpressions) {
    CreateJson(R"raw({
"stencils": {
    "HUD": "invalid expression @#$%"
}
})raw");
    PolyData data = {};
    EXPECT_FALSE(manager.IsHud(data));
}

TEST_F(PolyRoutingManagerTest, MissingStencils) {
    CreateJson(R"raw({
"other_key": {}
})raw");
    PolyData data = {};
    EXPECT_FALSE(manager.IsHud(data));
}
