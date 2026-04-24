#include <gtest/gtest.h>
#include "../../core/rend/vulkan/gbuffer/GbufferContext.h"
#include <fstream>

using namespace rend;

class GbufferContextTest : public ::testing::Test {
protected:
    GbufferContext context;
};

TEST_F(GbufferContextTest, UnifiedLoad) {
    std::string json = R"({
      "safe_zone": { "w": 640, "h": 480 },
      "hud_zones": [
        {
          "name": "zone_1",
          "w": 100, "h": 50,
          "source": { "x": 0, "y": 0, "anchor": "SCREEN_TOP_LEFT" },
          "mapping": { "x": 10, "y": 10, "anchor": "SCREEN_TOP_LEFT" }
        }
      ],
      "stencils": {
        "HUD": "texture_hash == 2748"
      }
    })";

    std::ofstream ofs("gbuffer_configuration.json");
    ofs << json;
    ofs.close();

    context.UpdateViewport(640, 480);
    context.LoadConfig("gbuffer_configuration.json");

    // Verify HudCompositor
    auto transforms = context.GetHudCompositor().getCachedTransforms();
    EXPECT_EQ(transforms.size(), 1);
    EXPECT_EQ(transforms[0].name, "zone_1");

    // Verify PolyRoutingManager
    PolyData data = {};
    data.texture_hash = 0xABC;
    EXPECT_TRUE(context.GetRoutingManager().IsHud(data));
}

TEST_F(GbufferContextTest, ViewportUpdate) {
    std::string json = R"({
      "safe_zone": { "w": 640, "h": 480 },
      "hud_zones": [
        {
          "name": "zone_1",
          "w": 640, "h": 480,
          "source": { "x": 0, "y": 0, "anchor": "SCREEN_TOP_LEFT" },
          "mapping": { "x": 0, "y": 0, "anchor": "SCREEN_TOP_LEFT" }
        }
      ]
    })";

    std::ofstream ofs("gbuffer_configuration.json");
    ofs << json;
    ofs.close();

    context.LoadConfig("gbuffer_configuration.json");
    
    // Test 640x480
    context.UpdateViewport(640, 480);
    auto transforms = context.GetHudCompositor().getCachedTransforms();
    ASSERT_EQ(transforms.size(), 1);
    EXPECT_FLOAT_EQ(transforms[0].destination.w, 640.0f);

    // Test 1280x960
    context.UpdateViewport(1280, 960);
    transforms = context.GetHudCompositor().getCachedTransforms();
    ASSERT_EQ(transforms.size(), 1);
    EXPECT_FLOAT_EQ(transforms[0].destination.w, 1280.0f);
}
