#include "HostRenderer.h"
#include "log/LogManager.h"
#include "cfg/option.h"
#include "hw/pvr/ta.h"
#include "hw/pvr/pvr_regs.h"
#include <iostream>
#include "wsi/context.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#if defined(USE_SDL)
#include <SDL.h>
#include <SDL_syswm.h>
#endif

#include "version.h"

Renderer* rend_HostRenderer() {
    return new rend::HostRenderer();
}

namespace rend {

HostRenderer::HostRenderer() {
    host_interface.get_game_id = GetGameId;
    host_interface.get_host_name = GetHostName;
    host_interface.get_host_version = GetHostVersion;
    host_interface.log = LogMessage;
}

HostRenderer::~HostRenderer() {
    Term();
}

bool HostRenderer::Init() {
    if (!loadPlugin()) {
        ERROR_LOG(RENDERER, "Failed to load plugin renderer. Running as norend fallback.");
        return true;
    }

    if (vtable) {
        INFO_LOG(RENDERER, "Loaded plugin: %s v%s (API v%d)", 
                 vtable->name ? vtable->name : "Unknown", 
                 vtable->version ? vtable->version : "0.0.0",
                 vtable->api_version);
    }

    FlycastWindowHandle windowHandle = getWindowHandle();

    if (vtable && vtable->init) {
        return vtable->init(reinterpret_cast<FlycastHostHandle>(this), &windowHandle, &host_interface);
    }
    return false;
}

void HostRenderer::Term() {
    if (vtable && vtable->term) {
        vtable->term();
    }
    unloadPlugin();
}

static FlycastCullMode MapCullMode(u32 pvrCullMode) {
    switch (pvrCullMode) {
        case 0: return FLYCAST_CULL_NONE;
        case 1: return FLYCAST_CULL_BACK;  // Culls CW (positive area)
        case 2: return FLYCAST_CULL_FRONT; // Culls CCW (negative area)
        default: return FLYCAST_CULL_NONE;
    }
}

static FlycastBlendFactor MapBlendFactor(u32 pvrBlendFactor) {
    return static_cast<FlycastBlendFactor>(pvrBlendFactor & 7);
}

static FlycastDepthFunc MapDepthFunc(u32 pvrDepthFunc) {
    return static_cast<FlycastDepthFunc>(pvrDepthFunc & 7);
}

void HostRenderer::Process(TA_context *ctx) {
    if (!vtable || !vtable->process) return;

    ta_parse(ctx, true);

    auto processBatches = [&](const std::vector<PolyParam>& polys) {
        for (const auto& poly : polys) {
            if (poly.count == 0) continue;

            PluginGeometryData data = {};
            data.vertices = reinterpret_cast<const PluginVertex*>(ctx->rend.verts.data());
            data.vertex_count = ctx->rend.verts.size();
            
            data.indices = &ctx->rend.idx[poly.first];
            data.index_count = poly.count;

            data.scissor_enable = true; 
            data.scissor_x = ctx->rend.fb_X_CLIP.min;
            data.scissor_y = ctx->rend.fb_Y_CLIP.min;
            data.scissor_w = (int32_t)ctx->rend.fb_X_CLIP.max - (int32_t)ctx->rend.fb_X_CLIP.min + 1;
            data.scissor_h = (int32_t)ctx->rend.fb_Y_CLIP.max - (int32_t)ctx->rend.fb_Y_CLIP.min + 1;

            if (data.scissor_w < 0) data.scissor_w = 0;
            if (data.scissor_h < 0) data.scissor_h = 0;

            data.cull_mode = MapCullMode(poly.isp.CullMode);

            // Texture state (API v6)
            // tex_data and palette are left null: VRAM decoding is deferred (future task).
            // We set tex_mode so plugins can at least detect textured geometry.
            if (poly.pcw.Texture) {
                if (poly.tcw.PixelFmt == PixelPal8) {
                    data.tex_mode = FLYCAST_TEX_PAL8;
                    data.tex_width  = 8u << poly.tsp.TexU; // PVR2: TexU encodes log2(width)-3
                    data.tex_height = 8u << poly.tsp.TexV;
                    // tex_data / palette: nullptr (VRAM decoding not yet implemented)
                }
                // Other texture formats: leave tex_mode = FLYCAST_TEX_NONE (zeroized)
            }

            data.src_blend = MapBlendFactor(poly.tsp.SrcInstr);
            data.dst_blend = MapBlendFactor(poly.tsp.DstInstr);
            data.depth_func = MapDepthFunc(poly.isp.DepthMode);
            data.depth_write = !poly.isp.ZWriteDis;
            data.offset_enable = poly.pcw.Offset;

            // Fog state (API v9)
            data.fog_mode = config::Fog ? poly.tsp.FogCtrl : 2;
            data.fog_color = FOG_COL_RAM.full;
            data.fog_vertex_color = FOG_COL_VERT.full;
            data.fog_density = FOG_DENSITY.get();
            data.fog_clamp_min = ctx->rend.fog_clamp_min.full;
            data.fog_clamp_max = ctx->rend.fog_clamp_max.full;
            data.fog_table = FOG_TABLE;

            vtable->process(&data);
        }
    };

    processBatches(ctx->rend.global_param_op);
    processBatches(ctx->rend.global_param_pt);
    processBatches(ctx->rend.global_param_tr);
}


bool HostRenderer::Render() {
    checkResize();
    if (vtable && vtable->render) {
        return vtable->render();
    }
    return true;
}

void HostRenderer::RenderFramebuffer(const FramebufferInfo& info) {
    if (!vtable || !vtable->render_framebuffer) return;

    PluginFramebufferInfo pluginInfo;
    pluginInfo.fb_r_sof1 = info.fb_r_sof1;
    pluginInfo.fb_r_sof2 = info.fb_r_sof2;
    
    pluginInfo.width = FB_X_CLIP.max + 1;
    if (FB_W_LINESTRIDE.stride != 0)
        pluginInfo.width = std::min((uint32_t)FB_W_LINESTRIDE.stride * 4, pluginInfo.width);
    
    pluginInfo.height = FB_Y_CLIP.max + 1;
    if (SCALER_CTL.vscalefactor < 0x400)
        pluginInfo.height = pluginInfo.height * 1024 / SCALER_CTL.vscalefactor;

    vtable->render_framebuffer(&pluginInfo);

}

bool HostRenderer::Present() {
    if (vtable && vtable->present) {
        return vtable->present();
    }
    return true;
}

bool HostRenderer::loadPlugin() {
    std::string pluginPath = config::PluginPath.get();
    if (pluginPath.empty()) {
#if defined(_WIN32)
        pluginPath = "flycast_dummy_plugin.dll";
#else
        pluginPath = "libflycast_dummy_plugin.so";
#endif
    }

#if defined(_WIN32)
    plugin_handle = LoadLibraryA(pluginPath.c_str());
    if (!plugin_handle) {
        ERROR_LOG(RENDERER, "Could not load %s", pluginPath.c_str());
        return false;
    }
    auto get_vtable = (const FlycastPluginVTable* (*)())GetProcAddress((HMODULE)plugin_handle, "flycast_plugin_get_vtable");
#else
    plugin_handle = dlopen(pluginPath.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!plugin_handle) {
        ERROR_LOG(RENDERER, "Could not load %s: %s", pluginPath.c_str(), dlerror());
        return false;
    }
    auto get_vtable = (const FlycastPluginVTable* (*)())dlsym(plugin_handle, "flycast_plugin_get_vtable");
#endif

    if (!get_vtable) {
        ERROR_LOG(RENDERER, "Could not find flycast_plugin_get_vtable symbol.");
        unloadPlugin();
        return false;
    }

    vtable = get_vtable();
    if (!vtable || vtable->api_version != FLYCAST_PLUGIN_API_VERSION) {
        ERROR_LOG(RENDERER, "Invalid plugin API version.");
        unloadPlugin();
        return false;
    }

    return true;
}

void HostRenderer::unloadPlugin() {
    if (plugin_handle) {
#if defined(_WIN32)
        FreeLibrary((HMODULE)plugin_handle);
#else
        dlclose(plugin_handle);
#endif
        plugin_handle = nullptr;
    }
    vtable = nullptr;
}

FlycastWindowHandle HostRenderer::getWindowHandle() {
    FlycastWindowHandle handle = {};
#if defined(USE_SDL)
    void* sdl_win = nullptr;
    void* unused = nullptr;
    if (GraphicsContext::Instance()) {
        GraphicsContext::Instance()->getWindow(&sdl_win, &unused);
    }

    if (sdl_win) {
        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (SDL_GetWindowWMInfo((SDL_Window*)sdl_win, &wmInfo)) {
#if defined(_WIN32)
            handle.os_type = FLYCAST_OS_WINDOWS;
            handle.window = (void*)wmInfo.info.win.window;
#elif defined(__APPLE__)
            handle.os_type = FLYCAST_OS_MACOS;
            handle.window = (void*)wmInfo.info.cocoa.window;
#elif defined(__ANDROID__)
            handle.os_type = FLYCAST_OS_ANDROID;
            handle.window = (void*)wmInfo.info.android.window;
#else
            if (wmInfo.subsystem == SDL_SYSWM_X11) {
                handle.os_type = FLYCAST_OS_X11;
                handle.display = (void*)wmInfo.info.x11.display;
                handle.window = (void*)wmInfo.info.x11.window;
            } else if (wmInfo.subsystem == SDL_SYSWM_WAYLAND) {
                handle.os_type = FLYCAST_OS_WAYLAND;
                handle.display = (void*)wmInfo.info.wl.display;
                handle.window = (void*)wmInfo.info.wl.surface;
            }
#endif
        }
    }
#endif
    return handle;
}

const char* HostRenderer::GetGameId(FlycastHostHandle host) {
    return config::Settings::instance().getGameId().c_str();
}

const char* HostRenderer::GetHostName(FlycastHostHandle host) {
    return "Flycast";
}

const char* HostRenderer::GetHostVersion(FlycastHostHandle host) {
    return GIT_VERSION;
}

void HostRenderer::LogMessage(FlycastHostHandle host, FlycastLogLevel level, const char* message) {
    LogTypes::LOG_LEVELS l = LogTypes::LINFO;
    switch (level) {
        case FLYCAST_LOG_DEBUG: l = LogTypes::LDEBUG; break;
        case FLYCAST_LOG_INFO:  l = LogTypes::LINFO;  break;
        case FLYCAST_LOG_WARN:  l = LogTypes::LWARNING; break;
        case FLYCAST_LOG_ERROR: l = LogTypes::LERROR; break;
    }
    GenericLog(l, LogTypes::RENDERER, __FILE__, __LINE__, "[Plugin] %s", message);
}

void HostRenderer::checkResize() {
    if (!vtable || !vtable->resize) return;

#if defined(USE_SDL)
    void* sdl_win = nullptr;
    void* unused = nullptr;
    if (GraphicsContext::Instance()) {
        GraphicsContext::Instance()->getWindow(&sdl_win, &unused);
    }

    if (sdl_win) {
        int w, h;
        SDL_GetWindowSize((SDL_Window*)sdl_win, &w, &h);
        if (w != (int)last_width || h != (int)last_height) {
            last_width = w;
            last_height = h;
            vtable->resize(w, h);
        }
    }
#endif
}

} // namespace rend
