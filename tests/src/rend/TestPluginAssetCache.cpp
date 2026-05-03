#include <gtest/gtest.h>
#include "rend/host/PluginAssetCache.h"

using namespace rend;

// Mock Plugin Implementation
static uint32_t last_created_handle = 0;
static uint32_t create_count = 0;
static uint32_t update_count = 0;
static uint32_t destroy_count = 0;

static uint32_t mock_create_texture(uint32_t width, uint32_t height, FlycastTexMode mode) {
    create_count++;
    return ++last_created_handle;
}

static void mock_update_texture(uint32_t handle, const uint8_t* data) {
    update_count++;
}

static void mock_destroy_texture(uint32_t handle) {
    destroy_count++;
}

class PluginAssetCacheTest : public ::testing::Test {
protected:
    FlycastPluginVTable vtable;
    const FlycastPluginVTable* vtable_ptr;

    void SetUp() override {
        vtable = {};
        vtable.create_texture = mock_create_texture;
        vtable.update_texture = mock_update_texture;
        vtable.destroy_texture = mock_destroy_texture;
        vtable_ptr = &vtable;
        
        last_created_handle = 0;
        create_count = 0;
        update_count = 0;
        destroy_count = 0;
    }
};

TEST_F(PluginAssetCacheTest, BasicCaching) {
    PluginAssetCache cache(vtable_ptr);
    uint8_t dummy_data[64];

    // First time: creation + update
    uint32_t h1 = cache.GetTexture(0x1000, 64, 64, FLYCAST_TEX_NONE, 1, 1, dummy_data);
    EXPECT_EQ(h1, 1);
    EXPECT_EQ(create_count, 1);
    EXPECT_EQ(update_count, 1);

    // Same texture, same version: no creation, no update
    uint32_t h2 = cache.GetTexture(0x1000, 64, 64, FLYCAST_TEX_NONE, 1, 2, dummy_data);
    EXPECT_EQ(h1, h2);
    EXPECT_EQ(create_count, 1);
    EXPECT_EQ(update_count, 1);

    // Same texture, new version: update only
    uint32_t h3 = cache.GetTexture(0x1000, 64, 64, FLYCAST_TEX_NONE, 2, 3, dummy_data);
    EXPECT_EQ(h1, h3);
    EXPECT_EQ(create_count, 1);
    EXPECT_EQ(update_count, 2);
}

TEST_F(PluginAssetCacheTest, TextureRecreation) {
    PluginAssetCache cache(vtable_ptr);
    uint8_t dummy_data[64];

    cache.GetTexture(0x1000, 64, 64, FLYCAST_TEX_NONE, 1, 1, dummy_data);
    EXPECT_EQ(create_count, 1);

    // Parameters change (size): destroy + create
    cache.GetTexture(0x1000, 128, 128, FLYCAST_TEX_NONE, 1, 2, dummy_data);
    EXPECT_EQ(create_count, 2);
    EXPECT_EQ(destroy_count, 1);

    // Parameters change (mode): destroy + create
    cache.GetTexture(0x1000, 128, 128, FLYCAST_TEX_PAL8, 1, 3, dummy_data);
    EXPECT_EQ(create_count, 3);
    EXPECT_EQ(destroy_count, 2);
}

TEST_F(PluginAssetCacheTest, GarbageCollection) {
    PluginAssetCache cache(vtable_ptr);
    uint8_t dummy_data[64];

    // Texture 1 used at frame 1
    cache.GetTexture(0x1000, 64, 64, FLYCAST_TEX_NONE, 1, 1, dummy_data);
    // Texture 2 used at frame 100
    cache.GetTexture(0x2000, 64, 64, FLYCAST_TEX_NONE, 1, 100, dummy_data);

    EXPECT_EQ(cache.GetTextureCount(), 2);

    // GC at frame 200 with age 120
    // Age of T1: 199 (> 120) -> Should be removed
    // Age of T2: 100 (<= 120) -> Should be kept
    cache.CollectGarbage(200, 120);

    EXPECT_EQ(cache.GetTextureCount(), 1);
    EXPECT_EQ(destroy_count, 1);

    // Use T2 at frame 250
    cache.GetTexture(0x2000, 64, 64, FLYCAST_TEX_NONE, 1, 250, dummy_data);
    
    // GC at frame 350 with age 120
    // Age of T2: 100 (<= 120) -> Should be kept
    cache.CollectGarbage(350, 120);
    EXPECT_EQ(cache.GetTextureCount(), 1);
}

TEST_F(PluginAssetCacheTest, Clear) {
    PluginAssetCache cache(vtable_ptr);
    uint8_t dummy_data[64];

    cache.GetTexture(0x1000, 64, 64, FLYCAST_TEX_NONE, 1, 1, dummy_data);
    cache.GetTexture(0x2000, 64, 64, FLYCAST_TEX_NONE, 1, 1, dummy_data);
    
    EXPECT_EQ(cache.GetTextureCount(), 2);
    cache.Clear();
    EXPECT_EQ(cache.GetTextureCount(), 0);
    EXPECT_EQ(destroy_count, 2);
}

TEST_F(PluginAssetCacheTest, NullPlugin) {
    const FlycastPluginVTable* null_vtable = nullptr;
    PluginAssetCache cache(null_vtable);
    uint8_t dummy_data[64];

    uint32_t handle = cache.GetTexture(0x1000, 64, 64, FLYCAST_TEX_NONE, 1, 1, dummy_data);
    EXPECT_EQ(handle, 0);
    EXPECT_EQ(create_count, 0);
}
