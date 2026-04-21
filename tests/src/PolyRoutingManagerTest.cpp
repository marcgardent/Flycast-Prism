#include <gtest/gtest.h>
#include "rend/PolyRoutingManager.h"
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

TEST_F(PolyRoutingManagerTest, BasicMatching) {
    CreateJson(R"({
"routing": [
  {
    "match": { "texHash": "0xABCDEF" },
    "actions": ["toHud"]
  },
  {
    "match": { "x": 10.0, "y": 20.0, "z": 0.5 },
    "actions": ["avoidDepth", "avoidNormal"]
  }
]
})");

    PolyRoutingManager::PolyMatchParams params = {0, 0, 0, 0, 0xABCDEF};
    u32 actions = manager.GetActions(params);
    EXPECT_TRUE(PolyRoutingManager::IsToHud(actions));
    EXPECT_FALSE(PolyRoutingManager::IsAvoidDepth(actions));

    params = {10.0f, 20.0f, 0.5f, 0, 0};
    actions = manager.GetActions(params);
    EXPECT_TRUE(PolyRoutingManager::IsAvoidDepth(actions));
    EXPECT_TRUE(PolyRoutingManager::IsAvoidNormal(actions));
    EXPECT_FALSE(PolyRoutingManager::IsToHud(actions));
}

TEST_F(PolyRoutingManagerTest, OverlappingRules) {
    CreateJson(R"({
"routing": [
  {
    "match": { "texHash": "0x123" },
    "actions": ["avoidAlbedo"]
  },
  {
    "match": { "texHash": "0x123" },
    "actions": ["avoidNormal"]
  }
]
})");

    PolyRoutingManager::PolyMatchParams params = {0, 0, 0, 0, 0x123};
    u32 actions = manager.GetActions(params);
    EXPECT_TRUE(PolyRoutingManager::IsAvoidAlbedo(actions));
    EXPECT_TRUE(PolyRoutingManager::IsAvoidNormal(actions));
}

TEST_F(PolyRoutingManagerTest, CoordinateTolerance) {
    CreateJson(R"({
"routing": [
  {
    "match": { "x": 10.0 },
    "action": "avoidDepth"
  }
]
})");

    // Exact match
    PolyRoutingManager::PolyMatchParams params = {10.0f, 0, 0, 0, 0};
    EXPECT_TRUE(PolyRoutingManager::IsAvoidDepth(manager.GetActions(params)));

    // Within tolerance (0.001)
    params.x = 10.0005f;
    EXPECT_TRUE(PolyRoutingManager::IsAvoidDepth(manager.GetActions(params)));

    // Outside tolerance
    params.x = 10.002f;
    EXPECT_FALSE(PolyRoutingManager::IsAvoidDepth(manager.GetActions(params)));
}

TEST_F(PolyRoutingManagerTest, HudPassPersistence) {
    CreateJson(R"({
"routing": [
  {
    "match": { "texHash": "0x123456" },
    "actions": ["startHudPass"]
  }
]
})");

    PolyRoutingManager::PolyMatchParams params = {0, 0, 0, 0, 0};
    EXPECT_FALSE(PolyRoutingManager::IsToHud(manager.GetActions(params)));

    // Trigger HUD pass
    params.texHash = 0x123456;
    u32 actions = manager.GetActions(params);
    EXPECT_TRUE(PolyRoutingManager::IsStartHudPass(actions));
    EXPECT_TRUE(PolyRoutingManager::IsToHud(actions));

    // Persists even without match
    params.texHash = 0;
    EXPECT_TRUE(PolyRoutingManager::IsToHud(manager.GetActions(params)));

    // Resets on NewFrame
    manager.NewFrame();
    EXPECT_FALSE(PolyRoutingManager::IsToHud(manager.GetActions(params)));
}

TEST_F(PolyRoutingManagerTest, MalformedJson) {
    // Missing 'routing' key
    CreateJson(R"({
"other_key": [
  { "match": { "texHash": "0x1" }, "action": "toHud" }
]
})");
    PolyRoutingManager::PolyMatchParams params = {0, 0, 0, 0, 0x1};
    EXPECT_FALSE(PolyRoutingManager::IsToHud(manager.GetActions(params)));

    // Invalid action string
    CreateJson(R"({
"routing": [
  { "match": { "texHash": "0x2" }, "action": "garbageAction" }
]
})");
    params.texHash = 0x2;
    EXPECT_EQ(manager.GetActions(params), Action_None);
}

TEST_F(PolyRoutingManagerTest, MultiActionList) {
    CreateJson(R"({
"routing": [
  {
    "match": { "count": 4 },
    "actions": ["avoidMotion", "avoidMaterial", "avoidNormal"]
  }
]
})");

    PolyRoutingManager::PolyMatchParams params = {0, 0, 0, 4, 0};
    u32 actions = manager.GetActions(params);
    EXPECT_TRUE(PolyRoutingManager::IsAvoidMotion(actions));
    EXPECT_TRUE(PolyRoutingManager::IsAvoidMaterial(actions));
    EXPECT_TRUE(PolyRoutingManager::IsAvoidNormal(actions));
    EXPECT_FALSE(PolyRoutingManager::IsAvoidAlbedo(actions));
}
