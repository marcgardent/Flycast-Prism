#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <chrono>

#include "../flycast_plugin_api.h"
#include "TestCase.h"
#include "TestManager.h"
#include "cases/TestGEO01.h"
#include "cases/TestGEO02.h"
#include "cases/TestGEO03.h"
#include "cases/TestGEO04.h"
#include "cases/TestTRN02.h"
#include "cases/TestSHD01.h"
#include "cases/TestTEX01.h"
#include "cases/TestTRN01.h"
#include "cases/TestSPE01.h"
#include "cases/TestSPE02.h"
#include "cases/TestOIT01.h"
#include "cases/TestGC01.h"

#include "BenchUI.h"
#include "cases/TestGEO06.h"
#include "cases/TestGEO07.h"

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

// PVR Register simulation for rendering logic
struct PVR_Regs {
    uint32_t fb_x_clip_max = 1279;
    uint32_t fb_y_clip_max = 719;
    uint32_t stride = 0;
    uint32_t v_scale = 0x400; // 1.0f
} g_pvr;

const char* bench_get_game_id(FlycastHostHandle host) { return "BENCHMARK"; }
const char* bench_get_host_name(FlycastHostHandle host) { return "FlycastBench"; }
const char* bench_get_host_version(FlycastHostHandle host) { return "1.1-bench"; }
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
    mgr.registerTest(std::make_unique<TestGEO04>());
    mgr.registerTest(std::make_unique<TestGEO06>());
    mgr.registerTest(std::make_unique<TestGEO07>());
    mgr.registerTest(std::make_unique<TestSHD01>());
    mgr.registerTest(std::make_unique<TestTEX01>());
    mgr.registerTest(std::make_unique<TestTRN01>());
    mgr.registerTest(std::make_unique<TestTRN02>());
    mgr.registerTest(std::make_unique<TestSPE01>());
    mgr.registerTest(std::make_unique<TestSPE02>());
    mgr.registerTest(std::make_unique<TestOIT01>());
    mgr.registerTest(std::make_unique<TestGC01>());

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

    const auto& allTests = TestManager::instance().getTests();
    TestCase* activeTest = nullptr;

    if (!preSelectedTestId.empty()) {
        activeTest = TestManager::instance().getTestById(preSelectedTestId);
        for (size_t i = 0; i < allTests.size(); i++) {
            if (allTests[i]->getId() == preSelectedTestId) { currentTestIdx = (int)i; break; }
        }
    } else if (!allTests.empty()) {
        activeTest = allTests[0].get();
    }

    if (!activeTest) return 1;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;

    // Enable MSAA
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    SDL_Window* window = SDL_CreateWindow("Flycast Benchmarker", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                           1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    BenchUI ui;
    if (!singleTestMode && ui.init("Flycast Benchmarker Controller", 600, 400)) g_ui = &ui;

    void* lib = LIB_LOAD(pluginPath.c_str());
    if (!lib) return 1;

    auto get_vtable = (const FlycastPluginVTable* (*)())LIB_GET_PROC(lib, "flycast_plugin_get_vtable");
    const FlycastPluginVTable* vtable = get_vtable ? get_vtable() : nullptr;

    if (!vtable || vtable->api_version != FLYCAST_PLUGIN_API_VERSION) return 1;

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);

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
    }
#endif

    if (!vtable->init((FlycastHostHandle)0xCAFE, &winHandle, &bench_host_if)) return 1;

    int lastW = 0, lastH = 0;
    SDL_GetWindowSize(window, &lastW, &lastH);
    if (vtable->resize) vtable->resize(lastW, lastH);

    bool running = true;
    uint32_t frame_count = 0;
    PerfMetrics perf;
    uint32_t last_palette_crc = 0;
    uint32_t last_fog_crc = 0;

    struct BenchTextureInfo {
        uint32_t handle;
        uint32_t last_frame_used;
    };
    std::map<const void*, BenchTextureInfo> bench_tex_cache;

    while (running) {
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
                    }
                }
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                } else if (!singleTestMode) {
                    if (event.key.keysym.sym == SDLK_RIGHT || event.key.keysym.sym == SDLK_SPACE) {
                        currentTestIdx = (currentTestIdx + 1) % allTests.size();
                        activeTest = allTests[currentTestIdx].get();
                        frame_count = 0; last_palette_crc = 0; last_fog_crc = 0;
                        SDL_SetWindowTitle(window, ("Flycast Benchmarker - " + activeTest->getId()).c_str());
                    } else if (event.key.keysym.sym == SDLK_LEFT) {
                        currentTestIdx = (currentTestIdx + (int)allTests.size() - 1) % allTests.size();
                        activeTest = allTests[currentTestIdx].get();
                        frame_count = 0; last_palette_crc = 0; last_fog_crc = 0;
                        SDL_SetWindowTitle(window, ("Flycast Benchmarker - " + activeTest->getId()).c_str());
                    }
                }
            }
            if (g_ui) ui.handleEvent(event);
        }

        if (g_ui) {
            int nextIdx = ui.render(allTests, currentTestIdx, frame_count, activeTest->getFrameCount(), perf);
            if (nextIdx == -1) running = false;
            else if (nextIdx != currentTestIdx) {
                currentTestIdx = nextIdx;
                activeTest = allTests[currentTestIdx].get();
                frame_count = 0;
                last_palette_crc = 0; last_fog_crc = 0;
                SDL_SetWindowTitle(window, ("Flycast Benchmarker - " + activeTest->getId()).c_str());
            }
        }

        // --- Rendering Phase ---
        auto frameStart = std::chrono::high_resolution_clock::now();
        if (continuousMode || (frame_count < activeTest->getFrameCount())) {
            perf.pluginTimeMs = 0;

            if (frame_count == 0) {
                char buf[128];
                snprintf(buf, sizeof(buf), "[Frame Render] Starting Test: %s", activeTest->getId().c_str());
                bench_log((FlycastHostHandle)0xCAFE, FLYCAST_LOG_INFO, buf);
            }

            activeTest->update(1.0f / activeTest->getTargetFPS());
            TestData testData;
            activeTest->prepare(testData);

            // 1. Palette Update (Global state)
            if (!testData.batches.empty() && !testData.batches[0].palette.empty()) {
                uint32_t crc = 0;
                const auto& pal = testData.batches[0].palette;
                for (size_t i = 0; i < pal.size(); ++i) crc ^= pal[i] + i;
                if (crc != last_palette_crc && vtable->update_palette) {
                    auto start = std::chrono::high_resolution_clock::now();
                    vtable->update_palette(pal.data());
                    perf.pluginTimeMs += std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start).count();
                    last_palette_crc = crc;
                }
            }

            // 2. Fog Table Update
            if (!testData.batches.empty() && !testData.batches[0].fogTable.empty()) {
                uint32_t crc = 0;
                const auto& fog = testData.batches[0].fogTable;
                for (size_t i = 0; i < fog.size(); ++i) crc ^= fog[i] + i;
                if (crc != last_fog_crc && vtable->update_fog_table) {
                    auto start = std::chrono::high_resolution_clock::now();
                    vtable->update_fog_table(fog.data());
                    perf.pluginTimeMs += std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start).count();
                    last_fog_crc = crc;
                }
            }

            // 3. Geometry Processing
            if (!vtable->process_mega_batch) {
                std::cerr << "Plugin does not support Mega-Batch API!" << std::endl;
                running = false;
                continue;
            }

            uint32_t frame_id = frame_count;
            auto resolveTexture = [&](DrawBatch& batch) -> uint32_t {
                if (batch.texData.empty()) return 0;
                const void* ptr = batch.texData.data();
                if (bench_tex_cache.find(ptr) == bench_tex_cache.end()) {
                    if (vtable->create_texture) {
                        uint32_t h = vtable->create_texture(batch.texWidth, batch.texHeight, batch.texMode);
                        if (vtable->update_texture) vtable->update_texture(h, static_cast<const uint8_t*>(ptr));
                        bench_tex_cache[ptr] = { h, frame_id };
                    }
                }
                bench_tex_cache[ptr].last_frame_used = frame_id;
                return bench_tex_cache[ptr].handle;
            };

            std::vector<PluginVertex> allVerts;
            std::vector<uint32_t> allIdx;
            std::map<FlycastListType, std::vector<FlycastDrawCommand>> listCmds;

            for (auto& batch : testData.batches) {
                // Simulate non-contiguous index buffer (Gap)
                for (int i = 0; i < 32; i++) allIdx.push_back(0); 

                FlycastDrawCommand cmd = {};
                cmd.texture_handle = resolveTexture(batch);
                cmd.index_offset = (uint32_t)allIdx.size();
                cmd.index_count = (uint32_t)batch.indices.size();

                // State Mapping (Same as HostRenderer)
                cmd.src_blend = batch.srcBlend;
                cmd.dst_blend = batch.dstBlend;
                cmd.depth_func = batch.depthFunc;
                cmd.depth_write = batch.depthWrite;
                cmd.cull_mode = batch.cullMode;
                cmd.scissor_enable = batch.scissorEnable;
                cmd.scissor_x = batch.scissorX; cmd.scissor_y = batch.scissorY;
                cmd.scissor_w = batch.scissorW; cmd.scissor_h = batch.scissorH;

                cmd.fog_mode = batch.fogMode;
                cmd.fog_color = batch.fogColor;
                cmd.fog_density = batch.fogDensity;

                uint32_t vBase = (uint32_t)allVerts.size();
                for (auto& v : batch.vertices) allVerts.push_back(v);
                for (auto i : batch.indices) {
                    if (i == 0xFFFFFFFF)
                        allIdx.push_back(0xFFFFFFFF);
                    else
                        allIdx.push_back(i + vBase);
                }

                listCmds[batch.listType].push_back(cmd);
            }

            for (auto& [type, cmds] : listCmds) {
                PluginMegaBatch mega = {};
                mega.vertices = allVerts.data();
                mega.vertex_count = allVerts.size();
                mega.indices = allIdx.data();
                mega.index_count = allIdx.size();
                mega.commands = cmds.data();
                mega.command_count = cmds.size();
                mega.list_type = type;

                auto start = std::chrono::high_resolution_clock::now();
                vtable->process_mega_batch(&mega);
                perf.pluginTimeMs += std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start).count();
            }

            // 4. Render Framebuffer (Simulates output to VRAM/screen)
            PluginFramebufferInfo fbInfo = {};
            fbInfo.width = g_pvr.fb_x_clip_max + 1;
            if (g_pvr.stride != 0) fbInfo.width = std::min(g_pvr.stride * 4, fbInfo.width);
            fbInfo.height = g_pvr.fb_y_clip_max + 1;
            if (g_pvr.v_scale < 0x400) fbInfo.height = fbInfo.height * 1024 / g_pvr.v_scale;

            if (vtable->render_framebuffer) {
                auto start = std::chrono::high_resolution_clock::now();
                vtable->render_framebuffer(&fbInfo);
                perf.pluginTimeMs += std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start).count();
            }

            // 5. Final Render & Present
            if (vtable->render) {
                auto start = std::chrono::high_resolution_clock::now();
                vtable->render();
                perf.pluginTimeMs += std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start).count();
            }

            if (vtable->present) {
                auto start = std::chrono::high_resolution_clock::now();
                vtable->present();
                perf.presentTimeMs = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - start).count();
                perf.pluginTimeMs += perf.presentTimeMs;
            }

            // --- Garbage Collection (Every 120 frames, like HostRenderer) ---
            if (frame_count % 120 == 0) {
                for (auto it = bench_tex_cache.begin(); it != bench_tex_cache.end(); ) {
                    if (frame_count - it->second.last_frame_used > 120) {
                        if (vtable->destroy_texture) vtable->destroy_texture(it->second.handle);
                        it = bench_tex_cache.erase(it);
                    } else ++it;
                }
            }

            perf.buildTimeMs = std::chrono::duration<double, std::milli>(std::chrono::high_resolution_clock::now() - frameStart).count();
            frame_count++;
        }

        if (singleTestMode && !continuousMode && frame_count >= activeTest->getFrameCount()) running = false;
        
        // Frame Limiter (declared by test)
        uint32_t targetFPS = activeTest->getTargetFPS();
        uint32_t targetMs = 1000 / targetFPS;
        auto frameEnd = std::chrono::high_resolution_clock::now();
        uint32_t elapsedMs = (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart).count();
        
        if (elapsedMs < targetMs) {
            SDL_Delay(targetMs - elapsedMs);
        } else {
            SDL_Delay(1);
        }
    }

    if (vtable->term) vtable->term();
    LIB_CLOSE(lib);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}