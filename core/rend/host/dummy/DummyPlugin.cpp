#include "../flycast_plugin_api.h"
#include <iostream>
#include <SDL.h>

#ifdef __linux__
#include <X11/Xlib.h>
#endif

static SDL_Window* sdl_window = NULL;
static SDL_Renderer* sdl_renderer = NULL;
static const FlycastHostInterface* host_if = NULL;
static FlycastHostHandle host_handle = NULL;

static bool dummy_init(FlycastHostHandle host, const FlycastWindowHandle* window, const FlycastHostInterface* _host_if) {
    host_if = _host_if;
    host_handle = host;
    
    if (host_if) {
        char buffer[256];
        const char* game_id = host_if->get_game_id(host_handle);
        const char* host_name = host_if->get_host_name(host_handle);
        const char* host_ver = host_if->get_host_version(host_handle);
        
        snprintf(buffer, sizeof(buffer), "Initializing on %s %s. Current Game ID: %s", 
                 host_name, host_ver, game_id && *game_id ? game_id : "None");
        host_if->log(host_handle, FLYCAST_LOG_INFO, buffer);
    }

    if (window->os_type == FLYCAST_OS_X11) {
        // On Linux/X11, we use Xlib directly to avoid SDL/Vulkan conflicts
        return true;
    }

    // Fallback SDL pour les autres OS (Windows, etc.)
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return false;
    sdl_window = SDL_CreateWindowFrom(window->window);
    if (!sdl_window) return false;
    sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_SOFTWARE);
    return sdl_renderer != NULL;
}

static void dummy_term() {
    if (host_if && host_handle) {
        host_if->log(host_handle, FLYCAST_LOG_INFO, "Terminating Dummy Plugin");
    }
    if (sdl_renderer) SDL_DestroyRenderer(sdl_renderer);
    if (sdl_window) SDL_DestroyWindow(sdl_window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void dummy_resize(uint32_t width, uint32_t height) {
    if (host_if && host_handle) {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "Window resized to %dx%d", width, height);
        host_if->log(host_handle, FLYCAST_LOG_INFO, buffer);
    }
    std::cout << "[DummyPlugin] Resize callback: " << width << "x" << height << std::endl;
}

static void dummy_update_palette(const uint32_t* palette_data) {
    if (host_if && host_handle) {
        host_if->log(host_handle, FLYCAST_LOG_INFO, "[DummyPlugin] update_palette called");
    }
}

static void dummy_update_fog_table(const uint32_t* fog_table_data) {
    if (host_if && host_handle) {
        host_if->log(host_handle, FLYCAST_LOG_INFO, "[DummyPlugin] update_fog_table called");
    }
}

static uint32_t dummy_create_texture(uint32_t width, uint32_t height, FlycastTexMode mode) {
    static uint32_t next_id = 1;
    if (host_if && host_handle) {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "[DummyPlugin] create_texture %ux%u mode=%d -> ID %u", width, height, (int)mode, next_id);
        host_if->log(host_handle, FLYCAST_LOG_INFO, buffer);
    }
    return next_id++;
}

static void dummy_update_texture(uint32_t handle, const uint8_t* data) {
    if (host_if && host_handle) {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "[DummyPlugin] update_texture ID %u", handle);
        host_if->log(host_handle, FLYCAST_LOG_DEBUG, buffer);
    }
}

static void dummy_destroy_texture(uint32_t handle) {
    if (host_if && host_handle) {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "[DummyPlugin] destroy_texture ID %u", handle);
        host_if->log(host_handle, FLYCAST_LOG_INFO, buffer);
    }
}

static void dummy_process(const PluginGeometryData* data) {
    if (host_if && host_handle) {
        host_if->log(host_handle, FLYCAST_LOG_DEBUG, "  [Process Batch]");
        
        char buffer[512];
        
        // Scissor
        if (data->scissor_enable) {
            snprintf(buffer, sizeof(buffer), "    - Scissor: %d,%d %dx%d", 
                     data->scissor_x, data->scissor_y, data->scissor_w, data->scissor_h);
        } else {
            snprintf(buffer, sizeof(buffer), "    - Scissor: DISABLED");
        }
        host_if->log(host_handle, FLYCAST_LOG_DEBUG, buffer);

        // Cull
        const char* cull_str = "Unknown";
        switch(data->cull_mode) {
            case FLYCAST_CULL_NONE:  cull_str = "NONE";  break;
            case FLYCAST_CULL_FRONT: cull_str = "FRONT"; break;
            case FLYCAST_CULL_BACK:  cull_str = "BACK";  break;
        }
        snprintf(buffer, sizeof(buffer), "    - Cull Mode: %s", cull_str);
        host_if->log(host_handle, FLYCAST_LOG_DEBUG, buffer);

        // Texture (Updated in v11)
        if (data->texture_handle != 0) {
            snprintf(buffer, sizeof(buffer), "    - Texture Handle: %u", data->texture_handle);
        } else {
            snprintf(buffer, sizeof(buffer), "    - Texture: NONE");
        }
        host_if->log(host_handle, FLYCAST_LOG_DEBUG, buffer);

        // Blend
        snprintf(buffer, sizeof(buffer), "    - Blend State: src=%d, dst=%d", (int)data->src_blend, (int)data->dst_blend);
        host_if->log(host_handle, FLYCAST_LOG_DEBUG, buffer);

        // Depth
        snprintf(buffer, sizeof(buffer), "    - Depth State: func=%d, write=%s", (int)data->depth_func, data->depth_write ? "true" : "false");
        host_if->log(host_handle, FLYCAST_LOG_DEBUG, buffer);

        // Offset Color
        snprintf(buffer, sizeof(buffer), "    - Offset Enable: %s", data->offset_enable ? "true" : "false");
        host_if->log(host_handle, FLYCAST_LOG_DEBUG, buffer);

        // Vertices/Indices
        snprintf(buffer, sizeof(buffer), "    - Geometry: %zu vertices, %zu indices", data->vertex_count, data->index_count);
        host_if->log(host_handle, FLYCAST_LOG_DEBUG, buffer);

        // Fog (Updated in v11)
        snprintf(buffer, sizeof(buffer), "    - Fog: mode=%u, color=0x%08X, vtx_color=0x%08X, density=%f, clamp=0x%08X..0x%08X",
                 data->fog_mode, data->fog_color, data->fog_vertex_color, data->fog_density, 
                 data->fog_clamp_min, data->fog_clamp_max);
        host_if->log(host_handle, FLYCAST_LOG_DEBUG, buffer);

        // List Type (Added in v10)
        const char* list_str = "Unknown";
        switch (data->list_type) {
            case FLYCAST_LIST_OPAQUE:        list_str = "OPAQUE"; break;
            case FLYCAST_LIST_PUNCH_THROUGH: list_str = "PUNCH_THROUGH"; break;
            case FLYCAST_LIST_TRANSLUCENT:   list_str = "TRANSLUCENT"; break;
        }
        snprintf(buffer, sizeof(buffer), "    - List Type: %s", list_str);
        host_if->log(host_handle, FLYCAST_LOG_DEBUG, buffer);
    }
}

static uint32_t dummy_get_capabilities() {
    return FLYCAST_CAP_MEGA_BATCH;
}

static void dummy_process_mega_batch(const PluginMegaBatch* batch) {
    if (host_if && host_handle) {
        char buffer[512];
        const char* list_str = "Unknown";
        switch (batch->list_type) {
            case FLYCAST_LIST_OPAQUE:        list_str = "OPAQUE"; break;
            case FLYCAST_LIST_PUNCH_THROUGH: list_str = "PUNCH_THROUGH"; break;
            case FLYCAST_LIST_TRANSLUCENT:   list_str = "TRANSLUCENT"; break;
        }
        
        snprintf(buffer, 512, "[Dummy] MegaBatch: %zu vertices, %zu indices, %zu commands, list=%d\n",
                 batch->vertex_count, batch->index_count, batch->command_count, batch->list_type);
        host_if->log(host_handle, FLYCAST_LOG_DEBUG, buffer);

        for (size_t i = 0; i < batch->command_count; i++) {
            const auto& cmd = batch->commands[i];
            snprintf(buffer, sizeof(buffer), "  Command %zu: offset=%u, count=%u, tex=%u, blend=%d/%d, depth=%d/%s, cull=%d",
                     i, cmd.index_offset, cmd.index_count, cmd.texture_handle, 
                     (int)cmd.src_blend, (int)cmd.dst_blend, (int)cmd.depth_func, 
                     cmd.depth_write ? "true" : "false", (int)cmd.cull_mode);
            host_if->log(host_handle, FLYCAST_LOG_DEBUG, buffer);
        }
    }
}

static bool dummy_render() { 
    if (host_if && host_handle) {
        host_if->log(host_handle, FLYCAST_LOG_DEBUG, "  [Render]");
    }
    return true; 
}
static void dummy_render_framebuffer(const PluginFramebufferInfo* info) {
    if (host_if && host_handle) {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "  [Render Framebuffer]: %dx%d", info->width, info->height);
        host_if->log(host_handle, FLYCAST_LOG_DEBUG, buffer);
    }
}

static bool dummy_present_callback(const FlycastWindowHandle* window) {
    if (window->os_type == FLYCAST_OS_X11) {
        Display* dpy = (Display*)window->display;
        Window win = (Window)window->window;
        if (dpy && win) {
            GC gc = XCreateGC(dpy, win, 0, NULL);
            XSetForeground(dpy, gc, 0x0000FF); // Blue (format may vary by visual, but 0x0000FF is often blue)
            XFillRectangle(dpy, win, gc, 0, 0, 4000, 4000);
            XFreeGC(dpy, gc);
            XFlush(dpy);
        }
        return true;
    }
    
    if (sdl_renderer) {
        SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 255, 255);
        SDL_RenderClear(sdl_renderer);
        SDL_RenderPresent(sdl_renderer);
    }
    return true;
}

// We need to keep track of the window handle for present
static FlycastWindowHandle last_window;
static bool dummy_init_wrapper(FlycastHostHandle host, const FlycastWindowHandle* window, const FlycastHostInterface* host_if) {
    last_window = *window;
    return dummy_init(host, window, host_if);
}

static bool dummy_present() {
    if (host_if && host_handle) {
        host_if->log(host_handle, FLYCAST_LOG_DEBUG, "  [Present]");
    }
    return dummy_present_callback(&last_window);
}


static const FlycastPluginVTable vtable = {
    sizeof(FlycastPluginVTable),
    12,
    "Dummy Renderer",
    "1.2.0",
    dummy_init_wrapper,

    dummy_term,
    dummy_resize,

    dummy_get_capabilities,
    dummy_process,
    dummy_process_mega_batch,

    dummy_render,
    dummy_render_framebuffer,
    dummy_present,

    dummy_update_palette,
    dummy_update_fog_table,
    dummy_create_texture,
    dummy_update_texture,
    dummy_destroy_texture
};

PLUGIN_EXPORT const FlycastPluginVTable* flycast_plugin_get_vtable(void) {
    return &vtable;
}
