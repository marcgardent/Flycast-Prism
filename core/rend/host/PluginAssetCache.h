#pragma once

#include "flycast_plugin_api.h"
#include <map>
#include <mutex>
#include <stdint.h>

namespace rend {

/**
 * Metadata for a texture resource managed by the plugin.
 */
struct TextureResource {
    uint32_t handle;
    uint32_t width;
    uint32_t height;
    FlycastTexMode mode;
    uint32_t last_updates_count; // Tracks poly.texture->Updates for dirty tracking
    uint32_t last_frame_used;    // Tracks the last frame this texture was requested
};

/**
 * Manages caching and garbage collection of plugin-side assets (textures, etc.)
 */
class PluginAssetCache {
public:
    /**
     * @param vtable Pointer to the plugin VTable. Can be null if plugin is not loaded.
     */
    PluginAssetCache(const FlycastPluginVTable*& vtable);
    ~PluginAssetCache();

    /**
     * Retrieves a texture handle for the given VRAM address.
     * Creates or updates the texture in the plugin if necessary.
     * 
     * @return The plugin-side texture handle, or 0 if creation failed.
     */
    uint32_t GetTexture(uint32_t vram_addr, uint32_t width, uint32_t height, FlycastTexMode mode, 
                        uint32_t updates, uint32_t current_frame, const uint8_t* data);
    
    /**
     * Removes textures that haven't been used for more than max_age frames.
     */
    void CollectGarbage(uint32_t current_frame, uint32_t max_age = 120);

    /**
     * Destroys all cached textures and clears the cache.
     */
    void Clear();

    /**
     * Returns the number of textures currently in the cache.
     */
    size_t GetTextureCount() const { return texture_cache.size(); }

private:
    const FlycastPluginVTable*& vtable;
    std::map<uint32_t, TextureResource> texture_cache;
    mutable std::mutex mutex;
};

} // namespace rend
