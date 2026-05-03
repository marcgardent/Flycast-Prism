#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include "../flycast_plugin_api.h"
#include "TestCase.h"
#include "TestManager.h"
#include "cases/TestGEO01.h"
#include "cases/TestGEO02.h"
#include "cases/TestGEO03.h"
#include "cases/TestSHD01.h"
#include "cases/TestTEX01.h"
#include "cases/TestTRN01.h"
#include "cases/TestSPE01.h"
#include "cases/TestSPE02.h"
#include "cases/TestOIT01.h"
#include "cases/TestGC01.h"
#include "BenchUI.h"

static BenchUI* g_ui = nullptr;
uint32_t palette32_ram[1024];
uint32_t FOG_TABLE[128];

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
    if (g_ui) g_ui->addLog(message);
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
    mgr.registerTest(std::make_unique<TestTRN01>());
    mgr.registerTest(std::make_unique<TestSPE01>());
    mgr.registerTest(std::make_unique<TestSPE02>());
    mgr.registerTest(std::make_unique<TestOIT01>());
    mgr.registerTest(std::make_unique<TestGC01>());
    mgr.registerTest(std::make_unique<TestEXTGEO03>());
}

static void print_usage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " <plugin_path> [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --test <id>    Run a specific test and exit automatically" << std::endl;
    std::cout << "  --continuous   Keep running tests after their defined duration" << std::endl;
    std::cout << "  --help, -h     Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "Available Tests:" << std::endl;
    auto& mgr = TestManager::instance();
    for (const auto& test : mgr.getTests()) {
        std::cout << "  " << test->getId() << ": " << test->getName() << std::endl;
    }
}

int main(int argc, char** argv) {
    registerAllTests();

    std::string pluginPath = "";
    std::string preSelectedTestId = "";
    bool continuousMode = false;
    bool singleTestMode = false;
    int currentTestIdx = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--test" && i + 1 < argc) {
            preSelectedTestId = argv[++i];
            singleTestMode = true;
        } else if (arg == "--continuous") {
            continuousMode = true;
        } else if (arg.rfind("--", 0) != 0 && pluginPath.empty()) {
            pluginPath = arg;
        }
    }

    if (pluginPath.empty()) {
        std::cerr << "Error: No plugin path provided." << std::endl;
        print_usage(argv[0]);
        return 1;
    }

    TestCase* activeTest = nullptr;
    const auto& allTests = TestManager::instance().getTests();
    
    if (!preSelectedTestId.empty()) {
        activeTest = TestManager::instance().getTestById(preSelectedTestId);
        if (!activeTest) {
            std::cerr << "Unknown test ID: " << preSelectedTestId << std::endl;
            return 1;
        }
        // Find the index for single test mode too
        for (size_t i = 0; i < allTests.size(); i++) {
            if (allTests[i]->getId() == preSelectedTestId) {
                currentTestIdx = (int)i;
                break;
            }
        }
    } else {
        if (!allTests.empty()) {
            activeTest = allTests[0].get();
            currentTestIdx = 0;
        } else {
            std::cerr << "No tests registered!" << std::endl;
            return 1;
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Enable MSAA for the main rendering window
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    SDL_Window* window = SDL_CreateWindow("Flycast Benchmarker",
                                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                           1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    BenchUI ui;
    if (!singleTestMode && ui.init("Flycast Benchmarker Controller", 600, 400)) {
        g_ui = &ui;
    }

    void* lib = LIB_LOAD(pluginPath.c_str());
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
    bool needsRender = true;
    uint32_t lastTicks = SDL_GetTicks();
    uint32_t frame_count = 0;

    while (running) {
        uint32_t currentTicks = SDL_GetTicks();
        float dt = (currentTicks - lastTicks) / 1000.0f;
        lastTicks = currentTicks;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
                    running = false;
                } else if (event.window.windowID == SDL_GetWindowID(window)) {
                    if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || 
                        event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        lastW = event.window.data1;
                        lastH = event.window.data2;
                        if (vtable->resize) vtable->resize(lastW, lastH);
                        needsRender = true;
                    }
                }
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (!singleTestMode) {
                    if (event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_SPACE) {
                        currentTestIdx = (currentTestIdx + 1) % allTests.size();
                        activeTest = allTests[currentTestIdx].get();
                        frame_count = 0;
                        needsRender = true;
                        std::cout << "Starting test: " << activeTest->getId() << " - " << activeTest->getName() << std::endl;
                        SDL_SetWindowTitle(window, ("Flycast Benchmarker - " + activeTest->getId()).c_str());
                    } else if (event.key.keysym.sym == SDLK_LEFT) {
                        currentTestIdx = (currentTestIdx + (int)allTests.size() - 1) % allTests.size();
                        activeTest = allTests[currentTestIdx].get();
                        frame_count = 0;
                        needsRender = true;
                        std::cout << "Starting test: " << activeTest->getId() << " - " << activeTest->getName() << std::endl;
                        SDL_SetWindowTitle(window, ("Flycast Benchmarker - " + activeTest->getId()).c_str());
                    }
                }
            }
            if (g_ui) ui.handleEvent(event);
        }

        if (g_ui) {
            int nextIdx = ui.render(allTests, currentTestIdx, frame_count, activeTest->getFrameCount());
            if (nextIdx == -1) {
                running = false;
            } else if (nextIdx != currentTestIdx) {
                currentTestIdx = nextIdx;
                activeTest = allTests[currentTestIdx].get();
                frame_count = 0;
                needsRender = true;
                std::cout << "Starting test: " << activeTest->getId() << " - " << activeTest->getName() << std::endl;
                SDL_SetWindowTitle(window, ("Flycast Benchmarker - " + activeTest->getId()).c_str());
            }
        }
        
        if (continuousMode || needsRender || (frame_count < activeTest->getFrameCount())) {
            {
                if (frame_count == 0) {
                    char buf[128];
                    snprintf(buf, sizeof(buf), "[Frame Render] Starting Test: %s", activeTest->getId().c_str());
                    bench_log(benchHandle, FLYCAST_LOG_INFO, buf);
                }
            }

            activeTest->update(dt);

            TestData testData;
            activeTest->prepare(testData);

            static uint32_t last_palette_crc = 0;
            static uint32_t last_fog_crc = 0;
            if (frame_count == 0) {
                last_palette_crc = 0;
                last_fog_crc = 0;
            }
            
            struct BenchTextureInfo {
                uint32_t handle;
                uint32_t last_frame_used;
            };
            static std::map<const void*, BenchTextureInfo> bench_tex_cache;
            if (frame_count < activeTest->getFrameCount()) {
                frame_count++;
            }



            // --- GC: Cleanup unused textures every 120 frames ---
            if (frame_count % 120 == 0) {
                for (auto it = bench_tex_cache.begin(); it != bench_tex_cache.end(); ) {
                    if (frame_count - it->second.last_frame_used > 120) {
                        if (vtable->destroy_texture) {
                            vtable->destroy_texture(it->second.handle);
                        }
                        it = bench_tex_cache.erase(it);
                    } else {
                        ++it;
                    }
                }
            }

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

                // --- v11 State Tracking ---

                // 1. Palette
                if (!batch.palette.empty()) {
                    uint32_t crc = 0;
                    for (size_t i = 0; i < batch.palette.size(); ++i) crc ^= batch.palette[i] + i;
                    if (crc != last_palette_crc && vtable->update_palette) {
                        vtable->update_palette(batch.palette.data());
                        last_palette_crc = crc;
                    }
                }

                // 2. Fog Table
                if (!batch.fogTable.empty()) {
                    uint32_t crc = 0;
                    for (auto c : batch.fogTable) crc ^= c;
                    if (crc != last_fog_crc && vtable->update_fog_table) {
                        vtable->update_fog_table(batch.fogTable.data());
                        last_fog_crc = crc;
                    }
                }

                // 3. Textures
                geom.texture_handle = 0;
                if (!batch.texData.empty()) {
                    const void* tex_ptr = batch.texData.data();
                    if (bench_tex_cache.find(tex_ptr) == bench_tex_cache.end()) {
                        if (vtable->create_texture) {
                            uint32_t h = vtable->create_texture(batch.texWidth, batch.texHeight, batch.texMode);
                            if (vtable->update_texture) {
                                vtable->update_texture(h, batch.texData.data());
                            }
                            bench_tex_cache[tex_ptr] = { h, frame_count };
                        }
                    }
                    bench_tex_cache[tex_ptr].last_frame_used = frame_count;
                    geom.texture_handle = bench_tex_cache[tex_ptr].handle;
                }

                geom.src_blend = batch.srcBlend;
                geom.dst_blend = batch.dstBlend;
                geom.depth_func = batch.depthFunc;
                geom.depth_write = batch.depthWrite;
                geom.offset_enable = batch.offsetEnable;

                // Fog (SPE-02)
                geom.fog_mode = batch.fogMode;
                geom.fog_color = batch.fogColor;
                geom.fog_vertex_color = batch.fogVertexColor;
                geom.fog_density = batch.fogDensity;
                geom.fog_clamp_min = batch.fogClampMin;
                geom.fog_clamp_max = batch.fogClampMax;

                // OIT (OIT-01)
                geom.list_type = batch.listType;

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
            needsRender = false;

            // Auto-exit in single test mode
            if (singleTestMode && !continuousMode && frame_count >= activeTest->getFrameCount()) {
                running = false;
            }
        }

        SDL_Delay(16);
    }

    g_ui = nullptr;
    ui.shutdown();

    if (vtable->term) {
        vtable->term();
    }

    LIB_CLOSE(lib);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
