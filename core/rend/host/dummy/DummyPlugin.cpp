#include "../flycast_plugin_api.h"
#include <iostream>
#include <SDL.h>

#ifdef __linux__
#include <X11/Xlib.h>
#endif

static SDL_Window* sdl_window = NULL;
static SDL_Renderer* sdl_renderer = NULL;

static bool dummy_init(FlycastHostHandle host, const FlycastWindowHandle* window) {
    std::cout << "[DummyPlugin] init() called. OS Type: " << window->os_type 
              << " Window Handle: " << window->window << std::endl;
    
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
    std::cout << "[DummyPlugin] term() called." << std::endl;
    if (sdl_renderer) SDL_DestroyRenderer(sdl_renderer);
    if (sdl_window) SDL_DestroyWindow(sdl_window);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void dummy_process(const PluginGeometryData* data) {}
static bool dummy_render() { return true; }
static void dummy_render_framebuffer(const PluginFramebufferInfo* info) {}

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
static bool dummy_init_wrapper(FlycastHostHandle host, const FlycastWindowHandle* window) {
    last_window = *window;
    return dummy_init(host, window);
}

static bool dummy_present() {
    return dummy_present_callback(&last_window);
}


static const FlycastPluginVTable vtable = {
    sizeof(FlycastPluginVTable),
    FLYCAST_PLUGIN_API_VERSION,
    dummy_init_wrapper,

    dummy_term,
    dummy_process,
    dummy_render,
    dummy_render_framebuffer,
    dummy_present
};

PLUGIN_EXPORT const FlycastPluginVTable* flycast_plugin_get_vtable(void) {
    return &vtable;
}

