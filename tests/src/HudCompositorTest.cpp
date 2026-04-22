#include <gtest/gtest.h>
#include "../../core/rend/vulkan/gbuffer/HudCompositor.h"

class HudCompositorTest : public ::testing::Test {
protected:
    HudCompositor compositor;
};

TEST_F(HudCompositorTest, LoadValidJson) {
    std::string json = R"({
      "safe_zone": { "w": 640, "h": 480 },
      "hud_zones": [
        {
          "name": "zone_1",
          "mapping": { "x": 10, "y": 10, "w": 100, "h": 50, "anchor": "SCREEN_TOP_LEFT", "scale": 1.0, "zen_mode": false }
        }
      ]
    })";
    EXPECT_TRUE(compositor.loadFromJson(json, 1280, 720));
    auto transforms = compositor.getTransforms();
    ASSERT_EQ(transforms.size(), 1);
    EXPECT_EQ(transforms[0].name, "zone_1");
    EXPECT_FALSE(transforms[0].zenMode);
}

TEST_F(HudCompositorTest, MismatchedSafeZone) {
    std::string json = R"({
      "safe_zone": { "w": 800, "h": 600 },
      "hud_zones": []
    })";
    EXPECT_FALSE(compositor.loadFromJson(json, 1280, 720));
}

TEST_F(HudCompositorTest, OverlappingZones) {
    std::string json = R"({
      "safe_zone": { "w": 640, "h": 480 },
      "hud_zones": [
        {
          "name": "zone_1",
          "mapping": { "x": 0, "y": 0, "w": 100, "h": 100, "anchor": "SCREEN_TOP_LEFT" }
        },
        {
          "name": "zone_2",
          "mapping": { "x": 50, "y": 50, "w": 100, "h": 100, "anchor": "SCREEN_TOP_LEFT" }
        }
      ]
    })";
    EXPECT_TRUE(compositor.loadFromJson(json, 640, 480));
    auto transforms = compositor.getTransforms();
    // zone_2 should be pruned because it overlaps with zone_1
    EXPECT_EQ(transforms.size(), 1);
    EXPECT_EQ(transforms[0].name, "zone_1");
}

TEST_F(HudCompositorTest, AnchorPositioning) {
    std::string json = R"({
      "safe_zone": { "w": 640, "h": 480 },
      "hud_zones": [
        {
          "name": "top_left",
          "mapping": { "x": 0, "y": 0, "w": 10, "h": 10, "anchor": "SCREEN_TOP_LEFT" }
        },
        {
          "name": "bottom_right",
          "mapping": { "x": -10, "y": -10, "w": 10, "h": 10, "anchor": "SCREEN_BOTTOM_RIGHT" }
        }
      ]
    })";
    // 640x480 -> scale 1.0
    EXPECT_TRUE(compositor.loadFromJson(json, 640, 480));
    auto transforms = compositor.getTransforms();
    ASSERT_EQ(transforms.size(), 2);

    // top_left at (0,0)
    EXPECT_FLOAT_EQ(transforms[0].viewportRect.x, 0.0f);
    EXPECT_FLOAT_EQ(transforms[0].viewportRect.y, 0.0f);

    // bottom_right at (640-10, 480-10) = (630, 470)
    EXPECT_FLOAT_EQ(transforms[1].viewportRect.x, 630.0f);
    EXPECT_FLOAT_EQ(transforms[1].viewportRect.y, 470.0f);
}

TEST_F(HudCompositorTest, Scaling) {
    std::string json = R"({
      "safe_zone": { "w": 640, "h": 480 },
      "hud_zones": [
        {
          "name": "zone",
          "mapping": { "x": 10, "y": 10, "w": 100, "h": 100, "anchor": "SCREEN_TOP_LEFT", "scale": 2.0 }
        }
      ]
    })";
    // 1280x960 -> scale 2.0 (since 960/480 = 2.0)
    EXPECT_TRUE(compositor.loadFromJson(json, 1280, 960));
    auto transforms = compositor.getTransforms();
    ASSERT_EQ(transforms.size(), 1);

    // Rect = aPos + (mapping.x * scale), aPos + (mapping.y * scale), mapping.w * (scale * mapping.scale)
    // x = 0 + 10 * 2.0 = 20
    // y = 0 + 10 * 2.0 = 20
    // w = 100 * (2.0 * 2.0) = 400
    // h = 100 * (2.0 * 2.0) = 400
    EXPECT_FLOAT_EQ(transforms[0].viewportRect.x, 20.0f);
    EXPECT_FLOAT_EQ(transforms[0].viewportRect.y, 20.0f);
    EXPECT_FLOAT_EQ(transforms[0].viewportRect.w, 400.0f);
    EXPECT_FLOAT_EQ(transforms[0].viewportRect.h, 400.0f);
}

TEST_F(HudCompositorTest, ZenMode) {
    std::string json = R"({
      "safe_zone": { "w": 640, "h": 480 },
      "hud_zones": [
        {
          "name": "zen",
          "mapping": { "x": 0, "y": 0, "w": 10, "h": 10, "zen_mode": true }
        },
        {
          "name": "normal",
          "mapping": { "x": 20, "y": 20, "w": 10, "h": 10, "zen_mode": false }
        }
      ]
    })";
    EXPECT_TRUE(compositor.loadFromJson(json, 640, 480));
    auto transforms = compositor.getTransforms();
    ASSERT_EQ(transforms.size(), 2);
    EXPECT_TRUE(transforms[0].zenMode);
    EXPECT_FALSE(transforms[1].zenMode);
}

TEST_F(HudCompositorTest, InvalidJson) {
    EXPECT_FALSE(compositor.loadFromJson("{ invalid }", 640, 480));
}

TEST_F(HudCompositorTest, SafeZonePositioning) {
    std::string json = R"({
      "safe_zone": { "w": 640, "h": 480 },
      "hud_zones": [
        {
          "name": "safe_top_left",
          "mapping": { "x": 0, "y": 0, "w": 10, "h": 10, "anchor": "SAFE_ZONE_TOP_LEFT" }
        }
      ]
    })";
    // 1280x480 (Ultrawide-ish)
    // scale = 480/480 = 1.0
    // safeW = 640 * 1.0 = 640
    // safeX = (1280 - 640) / 2 = 320
    EXPECT_TRUE(compositor.loadFromJson(json, 1280, 480));
    auto transforms = compositor.getTransforms();
    ASSERT_EQ(transforms.size(), 1);
    EXPECT_FLOAT_EQ(transforms[0].viewportRect.x, 320.0f);
    EXPECT_FLOAT_EQ(transforms[0].viewportRect.y, 0.0f);
}

TEST_F(HudCompositorTest, UnknownAnchor) {
    std::string json = R"({
      "safe_zone": { "w": 640, "h": 480 },
      "hud_zones": [
        {
          "name": "unknown",
          "mapping": { "x": 50, "y": 50, "w": 10, "h": 10, "anchor": "GARBAGE" }
        }
      ]
    })";
    EXPECT_TRUE(compositor.loadFromJson(json, 640, 480));
    auto transforms = compositor.getTransforms();
    ASSERT_EQ(transforms.size(), 1);
    // Should fallback to SCREEN_TOP_LEFT (0,0)
    EXPECT_FLOAT_EQ(transforms[0].viewportRect.x, 50.0f);
    EXPECT_FLOAT_EQ(transforms[0].viewportRect.y, 50.0f);
}
