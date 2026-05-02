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
        // Sur Linux/X11, on va utiliser Xlib directement pour éviter les conflits SDL/Vulkan
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

static void dummy_process(const PluginGeometryData* data) {}
static bool dummy_render() { return true; }
static void dummy_render_framebuffer(const PluginFramebufferInfo* info) {
    // Only log once to avoid spamming
    static bool logged_fb = false;
    if (!logged_fb && host_if && host_handle) {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "Render Framebuffer called: %dx%d", info->width, info->height);
        host_if->log(host_handle, FLYCAST_LOG_DEBUG, buffer);
        logged_fb = true;
    }
}

static bool dummy_present_callback(const FlycastWindowHandle* window) {
    if (window->os_type == FLYCAST_OS_X11) {
        Display* dpy = (Display*)window->display;
        Window win = (Window)window->window;
        if (dpy && win) {
            GC gc = XCreateGC(dpy, win, 0, NULL);
            XSetForeground(dpy, gc, 0x0000FF); // Bleu (format peut varier selon le visual, mais 0x0000FF est souvent bleu)
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

// On a besoin de garder une trace du window handle pour le present
static FlycastWindowHandle last_window;
static bool dummy_init_wrapper(FlycastHostHandle host, const FlycastWindowHandle* window, const FlycastHostInterface* host_if) {
    last_window = *window;
    return dummy_init(host, window, host_if);
}

static bool dummy_present() {
    return dummy_present_callback(&last_window);
}


static const FlycastPluginVTable vtable = {
    sizeof(FlycastPluginVTable),
    FLYCAST_PLUGIN_API_VERSION,
    "Dummy Renderer",
    "1.0.0",
    dummy_init_wrapper,

    dummy_term,
    dummy_resize,
    dummy_process,
    dummy_render,
    dummy_render_framebuffer,
    dummy_present
};

PLUGIN_EXPORT const FlycastPluginVTable* flycast_plugin_get_vtable(void) {
    return &vtable;
}

