#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include "../flycast_plugin_api.h"
#include "TestCase.h"
#include "TestManager.h"
#include "tests.h"

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

void registerAllTests() {
    auto& mgr = TestManager::instance();
    mgr.registerTest(std::make_unique<TestGEO01>());
    mgr.registerTest(std::make_unique<TestGEO02>());
    mgr.registerTest(std::make_unique<TestGEO03>());
    mgr.registerTest(std::make_unique<TestSHD01>());
    mgr.registerTest(std::make_unique<TestTEX01>());
}

void printHelp(const char* progName) {
    std::cout << "Usage: " << progName << " <plugin_path> [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --test <id>    Pre-select a test case (e.g., GEO-01)" << std::endl;
    std::cout << "  --list         List available test cases" << std::endl;
}

int main(int argc, char** argv) {
    registerAllTests();

    if (argc < 2) {
        printHelp(argv[0]);
        return 1;
    }

    const char* pluginPath = argv[1];
    std::string preSelectedTestId = "";

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--test" && i + 1 < argc) {
            preSelectedTestId = argv[++i];
        } else if (arg == "--list") {
            std::cout << "Available tests:" << std::endl;
            for (auto& t : TestManager::instance().getTests()) {
                std::cout << "  " << t->getId() << ": " << t->getName() << " - " << t->getDescription() << std::endl;
            }
            return 0;
        }
    }

    TestCase* activeTest = nullptr;
    const auto& allTests = TestManager::instance().getTests();
    
    if (!preSelectedTestId.empty()) {
        activeTest = TestManager::instance().getTestById(preSelectedTestId);
        if (!activeTest) {
            std::cerr << "Unknown test ID: " << preSelectedTestId << std::endl;
            return 1;
        }
    } else {
        // Default to the first registered test without any user interaction
        if (!allTests.empty()) {
            activeTest = allTests[0].get();
        } else {
            std::cerr << "No tests registered!" << std::endl;
            return 1;
        }
    }

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

    FlycastHostHandle benchHandle = (FlycastHostHandle)0xCAFE;

    if (!vtable->init(benchHandle, &winHandle, &bench_host_if)) {
        std::cerr << "Plugin init failed" << std::endl;
        LIB_CLOSE(lib);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    int lastW = 0, lastH = 0;
    SDL_GetWindowSize(window, &lastW, &lastH);
    if (vtable->resize) {
        vtable->resize(lastW, lastH);
    }

    std::cout << "Starting test: " << activeTest->getId() << " - " << activeTest->getName() << std::endl;
    {
        std::string title = "Flycast Benchmarker - " + activeTest->getId();
        SDL_SetWindowTitle(window, title.c_str());
    }

    bool running = true;
    uint32_t lastTicks = SDL_GetTicks();
    while (running) {
        uint32_t currentTicks = SDL_GetTicks();
        float dt = (currentTicks - lastTicks) / 1000.0f;
        lastTicks = currentTicks;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_WINDOWEVENT && 
                       (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || 
                        event.window.event == SDL_WINDOWEVENT_RESIZED)) {
                lastW = event.window.data1;
                lastH = event.window.data2;
                if (vtable->resize) {
                    vtable->resize(lastW, lastH);
                }
            } else if (event.type == SDL_KEYDOWN) {
                size_t currentIndex = 0;
                for (size_t i = 0; i < allTests.size(); ++i) {
                    if (allTests[i].get() == activeTest) {
                        currentIndex = i;
                        break;
                    }
                }

                if (event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_n) {
                    currentIndex = (currentIndex + 1) % allTests.size();
                    activeTest = allTests[currentIndex].get();
                    std::cout << "Switched to: " << activeTest->getId() << " - " << activeTest->getName() << std::endl;
                    std::string title = "Flycast Benchmarker - " + activeTest->getId();
                    SDL_SetWindowTitle(window, title.c_str());
                } else if (event.key.keysym.sym == SDLK_LEFT || event.key.keysym.sym == SDLK_p) {
                    currentIndex = (currentIndex + allTests.size() - 1) % allTests.size();
                    activeTest = allTests[currentIndex].get();
                    std::cout << "Switched to: " << activeTest->getId() << " - " << activeTest->getName() << std::endl;
                    std::string title = "Flycast Benchmarker - " + activeTest->getId();
                    SDL_SetWindowTitle(window, title.c_str());
                }
            }
        }

        activeTest->update(dt);

        TestData testData;
        activeTest->prepare(testData);

        for (auto& batch : testData.batches) {
            PluginGeometryData geom = {};
            geom.vertices = batch.vertices.data();
            geom.vertex_count = (uint32_t)batch.vertices.size();
            geom.indices = batch.indices.data();
            geom.index_count = (uint32_t)batch.indices.size();
            geom.scissor_enable = batch.scissorEnable;
            geom.scissor_x = batch.scissorX;
            geom.scissor_y = batch.scissorY;
            geom.scissor_w = batch.scissorW;
            geom.scissor_h = batch.scissorH;
            geom.cull_mode = batch.cullMode;
            // Texture (TEX-01)
            geom.tex_mode   = batch.texMode;
            geom.tex_width  = batch.texWidth;
            geom.tex_height = batch.texHeight;
            geom.tex_data   = batch.texData.empty()   ? nullptr : batch.texData.data();
            geom.palette    = batch.palette.empty()   ? nullptr : batch.palette.data();

            if (vtable->process) {
                vtable->process(&geom);
            }
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

        SDL_Delay(16);
    }

    if (vtable->term) {
        vtable->term();
    }

    LIB_CLOSE(lib);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
