#include "PluginAssetCache.h"

namespace rend {

PluginAssetCache::PluginAssetCache(const FlycastPluginVTable*& vtable) : vtable(vtable) {
}

PluginAssetCache::~PluginAssetCache() {
    Clear();
}

uint32_t PluginAssetCache::GetTexture(uint32_t vram_addr, uint32_t width, uint32_t height, FlycastTexMode mode, 
                                     uint32_t updates, uint32_t current_frame, const uint8_t* data) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!vtable) return 0;

    auto it = texture_cache.find(vram_addr);
    bool needs_push = false;

    if (it == texture_cache.end() || it->second.width != width || it->second.height != height || it->second.mode != mode) {
        // New texture or changed parameters
        if (it != texture_cache.end()) {
            if (vtable->destroy_texture) {
                vtable->destroy_texture(it->second.handle);
            }
            texture_cache.erase(it);
        }

        if (vtable->create_texture) {
            uint32_t handle = vtable->create_texture(width, height, mode);
            if (handle != 0) {
                texture_cache[vram_addr] = { handle, width, height, mode, 0, current_frame };
                it = texture_cache.find(vram_addr);
                needs_push = true;
            } else {
                return 0;
            }
        } else {
            return 0;
        }
    } else {
        // Existing texture, check for updates
        if (updates > it->second.last_updates_count) {
            needs_push = true;
        }
        it->second.last_frame_used = current_frame;
    }

    if (needs_push && it != texture_cache.end()) {
        if (vtable->update_texture) {
            vtable->update_texture(it->second.handle, data);
        }
        it->second.last_updates_count = updates;
    }

    return it != texture_cache.end() ? it->second.handle : 0;
}

void PluginAssetCache::CollectGarbage(uint32_t current_frame, uint32_t max_age) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!vtable) return;

    for (auto it = texture_cache.begin(); it != texture_cache.end(); ) {
        if (current_frame - it->second.last_frame_used > max_age) {
            if (vtable->destroy_texture) {
                vtable->destroy_texture(it->second.handle);
            }
            it = texture_cache.erase(it);
        } else {
            ++it;
        }
    }
}

void PluginAssetCache::Clear() {
    std::lock_guard<std::mutex> lock(mutex);
    if (vtable && vtable->destroy_texture) {
        for (auto const& [addr, res] : texture_cache) {
            vtable->destroy_texture(res.handle);
        }
    }
    texture_cache.clear();
}

} // namespace rend
