#include <gtest/gtest.h>
#include "../../core/rend/vulkan/gbuffer/HudCompositor.h"

// Helper macro for testing float equality in our Rect struct
#define EXPECT_RECT_EQ(rect, _x, _y, _w, _h) \
    EXPECT_FLOAT_EQ((rect).x, (_x)); \
    EXPECT_FLOAT_EQ((rect).y, (_y)); \
    EXPECT_FLOAT_EQ((rect).w, (_w)); \
    EXPECT_FLOAT_EQ((rect).h, (_h))

class HudCompositorTest : public ::testing::Test {
protected:
    HudCompositor compositor;

    // Helper to generate a valid base JSON envelope
    std::string wrapZones(const std::string& zonesJson, float safeW = 640.0f, float safeH = 480.0f) {
        return R"({
            "safe_zone": { "w": )" + std::to_string(safeW) + R"(, "h": )" + std::to_string(safeH) + R"( },
            "hud_zones": [)" + zonesJson + R"(]
        })";
    }
};

// 1. Test basic JSON loading and coordinate scaling
TEST_F(HudCompositorTest, ValidJsonAndScaling) {
    std::string json = wrapZones(R"(
        {
            "name": "zone1",
            "w": 50, "h": 50,
            "zen_mode": true,
            "source": { "x": 10, "y": 20, "anchor": "SCREEN_TOP_LEFT" },
            "mapping": { "x": 100, "y": 200, "anchor": "SCREEN_TOP_LEFT" }
        }
    )");

    // Test with a 1280x960 resolution (scale factor = 2.0 compared to 640x480 VIRT)
    EXPECT_TRUE(compositor.loadFromJson(json, 1280.0f, 960.0f)) << json;

    const auto& transforms = compositor.getCachedTransforms();
    ASSERT_EQ(transforms.size(), 1);

    const auto& t = transforms[0];
    EXPECT_EQ(t.name, "zone1");
    EXPECT_TRUE(t.zenMode);

    // Scale is 2.0, SCREEN_TOP_LEFT is (0,0)
    // Source should be scaled: x=20, y=40, w=100, h=100
    EXPECT_RECT_EQ(t.source, 20.0f, 40.0f, 100.0f, 100.0f);

    // Mapping should be scaled: x=200, y=400, w=100, h=100
    EXPECT_RECT_EQ(t.destination, 200.0f, 400.0f, 100.0f, 100.0f);
}

// 2. Test center anchoring
TEST_F(HudCompositorTest, AnchorCalculations) {
    std::string json = wrapZones(R"(
        {
            "name": "center_zone",
            "w": 10, "h": 10,
            "source": { "x": 0, "y": 0, "anchor": "SCREEN_CENTER" },
            "mapping": { "x": 0, "y": 0, "anchor": "SCREEN_CENTER" }
        }
    )");

    // Resolution 1920x1080 (Scale factor = 1080 / 480 = 2.25)
    EXPECT_TRUE(compositor.loadFromJson(json, 1920.0f, 1080.0f));

    const auto& transforms = compositor.getCachedTransforms();
    ASSERT_EQ(transforms.size(), 1);

    // Center of 1920x1080 is (960, 540)
    // Size is 10 * 2.25 = 22.5
    EXPECT_RECT_EQ(transforms[0].source, 960.0f, 540.0f, 22.5f, 22.5f);
}

// 3. EDGE CASE: Malformed JSON
TEST_F(HudCompositorTest, MalformedJsonReturnsFalse) {
    EXPECT_FALSE(compositor.loadFromJson("{ bad json ]", 800.0f, 600.0f));
}

// 4. EDGE CASE: Wrong Safe Zone dimensions
TEST_F(HudCompositorTest, WrongSafeZoneReturnsFalse) {
    std::string json = wrapZones("", 800.0f, 600.0f); // Expected is 640x480
    EXPECT_FALSE(compositor.loadFromJson(json, 1280.0f, 960.0f));
}

// 5. EDGE CASE: Dimension mismatch check is removed as dimensions are now shared
// (Test removed)


// 6. EDGE CASE: Missing fields
TEST_F(HudCompositorTest, MissingFieldsAreSkipped) {
    std::string json = wrapZones(R"(
        {
            "name": "no_mapping",
            "w": 10, "h": 10,
            "source": { "x": 0, "y": 0 }
        },
        {
            "name": "valid",
            "w": 10, "h": 10,
            "source": { "x": 0, "y": 0 },
            "mapping": { "x": 50, "y": 50 }
        }
    )");

    EXPECT_TRUE(compositor.loadFromJson(json, 1280.0f, 960.0f));

    const auto& transforms = compositor.getCachedTransforms();
    ASSERT_EQ(transforms.size(), 1); // Only the valid one should remain
    EXPECT_EQ(transforms[0].name, "valid");
}

// 7. EDGE CASE: Collision/Overlap on Mapping destination
TEST_F(HudCompositorTest, OverlappingDestinationsArePruned) {
    std::string json = wrapZones(R"(
        {
            "name": "first_zone",
            "w": 100, "h": 100,
            "source": { "x": 0, "y": 0, "anchor": "SCREEN_TOP_LEFT" },
            "mapping": { "x": 0, "y": 0, "anchor": "SCREEN_TOP_LEFT" }
        },
        {
            "name": "second_zone_overlapping",
            "w": 100, "h": 100,
            "source": { "x": 500, "y": 500, "anchor": "SCREEN_TOP_LEFT" },
            "mapping": { "x": 50, "y": 50, "anchor": "SCREEN_TOP_LEFT" }
        }
    )");

    EXPECT_TRUE(compositor.loadFromJson(json, 1280.0f, 960.0f));

    const auto& transforms = compositor.getCachedTransforms();
    ASSERT_EQ(transforms.size(), 1);

    // The second zone mapping (x:50, y:50, w:100, h:100) intersects
    // the first one (x:0, y:0, w:100, h:100). It must be pruned.
    EXPECT_EQ(transforms[0].name, "first_zone");
}

// 8. Test Viewport Updates
TEST_F(HudCompositorTest, ViewportUpdateRecalculatesTransforms) {
    std::string json = wrapZones(R"(
        {
            "name": "resize_zone",
            "w": 50, "h": 50,
            "source": { "x": 10, "y": 10 },
            "mapping": { "x": 10, "y": 10 }
        }
    )");

    EXPECT_TRUE(compositor.loadFromJson(json, 640.0f, 480.0f));

    auto transforms = compositor.getCachedTransforms();
    ASSERT_EQ(transforms.size(), 1);
    EXPECT_RECT_EQ(transforms[0].destination, 10.0f, 10.0f, 50.0f, 50.0f);

    // Simulate window resize
    compositor.updateViewport(1280.0f, 960.0f);

    transforms = compositor.getCachedTransforms();
    ASSERT_EQ(transforms.size(), 1);
    // Everything should be multiplied by 2
    EXPECT_RECT_EQ(transforms[0].destination, 20.0f, 20.0f, 100.0f, 100.0f);
}