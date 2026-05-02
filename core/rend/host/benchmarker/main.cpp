#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <iostream>
#include <vector>
#include "../flycast_plugin_api.h"

#if defined(_WIN32)
#include <windows.h>
#define LIB_LOAD(path) LoadLibraryA(path)
#define LIB_GET_PROC(lib, name) GetProcAddress((HMODULE)lib, name)
#define LIB_CLOSE(lib) FreeLibrary((HMODULE)lib)
#else
#include <dlfcn.h>
#define LIB_LOAD(path) dlopen(path, RTLD_LAZY | RTLD_LOCAL)
#define LIB_GET_PROC(lib, name) dlsym(lib, name)
#define LIB_CLOSE(lib) dlclose(lib)
#endif

const char* bench_get_game_id(FlycastHostHandle host) { return "BENCHMARK"; }
const char* bench_get_host_name(FlycastHostHandle host) { return "FlycastBench"; }
const char* bench_get_host_version(FlycastHostHandle host) { return "1.0-bench"; }
void bench_log(FlycastHostHandle host, FlycastLogLevel level, const char* message) {
    std::cout << "[BenchLog] " << message << std::endl;
}

static FlycastHostInterface bench_host_if = {
    bench_get_game_id,
    bench_get_host_name,
    bench_get_host_version,
    bench_log
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <plugin_path>" << std::endl;
        return 1;
    }

    const char* pluginPath = argv[1];

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Flycast Benchmarker",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                          1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    void* lib = LIB_LOAD(pluginPath);
    if (!lib) {
#if !defined(_WIN32)
        std::cerr << "Could not load plugin: " << dlerror() << std::endl;
#else
        std::cerr << "Could not load plugin: " << pluginPath << std::endl;
#endif
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    auto get_vtable = (const FlycastPluginVTable* (*)())LIB_GET_PROC(lib, "flycast_plugin_get_vtable");
    if (!get_vtable) {
        std::cerr << "Could not find flycast_plugin_get_vtable symbol" << std::endl;
        LIB_CLOSE(lib);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const FlycastPluginVTable* vtable = get_vtable();
    if (!vtable) {
        std::cerr << "vtable is null" << std::endl;
        LIB_CLOSE(lib);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    if (vtable->api_version != FLYCAST_PLUGIN_API_VERSION) {
        std::cerr << "Invalid plugin API version: expected " << FLYCAST_PLUGIN_API_VERSION 
                  << ", got " << vtable->api_version << std::endl;
        LIB_CLOSE(lib);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::cout << "Loaded plugin: " << (vtable->name ? vtable->name : "Unknown") 
              << " v" << (vtable->version ? vtable->version : "0.0.0") << std::endl;

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
        std::cerr << "SDL_GetWindowWMInfo Error: " << SDL_GetError() << std::endl;
        LIB_CLOSE(lib);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    FlycastWindowHandle winHandle = {};
#if defined(_WIN32)
    winHandle.os_type = FLYCAST_OS_WINDOWS;
    winHandle.window = wmInfo.info.win.window;
#elif defined(__linux__)
    if (wmInfo.subsystem == SDL_SYSWM_X11) {
        winHandle.os_type = FLYCAST_OS_X11;
        winHandle.display = wmInfo.info.x11.display;
        winHandle.window = (void*)(uintptr_t)wmInfo.info.x11.window;
    } else if (wmInfo.subsystem == SDL_SYSWM_WAYLAND) {
        winHandle.os_type = FLYCAST_OS_WAYLAND;
        winHandle.display = wmInfo.info.wl.display;
        winHandle.window = wmInfo.info.wl.surface;
    } else {
        std::cerr << "Unsupported SDL subsystem: " << wmInfo.subsystem << std::endl;
        LIB_CLOSE(lib);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
#endif

    if (!vtable->init(nullptr, &winHandle, &bench_host_if)) {
        std::cerr << "Plugin init failed" << std::endl;
        LIB_CLOSE(lib);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    int lastW = 0, lastH = 0;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);
        if (winW != lastW || winH != lastH) {
            lastW = winW;
            lastH = winH;
            if (vtable->resize) {
                vtable->resize(winW, winH);
            }
        }

        // Blue Quad
        PluginVertex vertices[4] = {};
        
        // Define a quad in normalized coordinates or 640x480?
        // Let's use something that looks like a centered quad.
        float centerX = 320.0f;
        float centerY = 240.0f;
        float size = 100.0f;

        // TL
        vertices[0].x = centerX - size; vertices[0].y = centerY - size; vertices[0].z = 0.5f;
        // TR
        vertices[1].x = centerX + size; vertices[1].y = centerY - size; vertices[1].z = 0.5f;
        // BR
        vertices[2].x = centerX + size; vertices[2].y = centerY + size; vertices[2].z = 0.5f;
        // BL
        vertices[3].x = centerX - size; vertices[3].y = centerY + size; vertices[3].z = 0.5f;

        for (int i = 0; i < 4; ++i) {
            vertices[i].col[0] = 0;   // R
            vertices[i].col[1] = 0;   // G
            vertices[i].col[2] = 255; // B
            vertices[i].col[3] = 255; // A
        }

        uint32_t indices[6] = {0, 1, 2, 0, 2, 3};

        PluginGeometryData geom = {};
        geom.vertices = vertices;
        geom.vertex_count = 4;
        geom.indices = indices;
        geom.index_count = 6;

        if (vtable->process) {
            vtable->process(&geom);
        }

        PluginFramebufferInfo fbInfo = { 0, 0, (uint32_t)lastW, (uint32_t)lastH };
        
        if (vtable->render_framebuffer) {
            vtable->render_framebuffer(&fbInfo);
        }

        if (vtable->render) {
            vtable->render();
        }

        if (vtable->present) {
            vtable->present();
        }

        SDL_Delay(16); // ~60fps
    }

    if (vtable->term) {
        vtable->term();
    }

    LIB_CLOSE(lib);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
