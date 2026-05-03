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
#include "rend/TexCache.h"
#include "rend/texconv.h"
#include "hw/pvr/pvr_mem.h"

Renderer* rend_HostRenderer() {
    return new rend::HostRenderer();
}

namespace rend {

HostRenderer::HostRenderer() : asset_cache(vtable) {
    host_interface.get_game_id = GetGameId;
    host_interface.get_host_name = GetHostName;
    host_interface.get_host_version = GetHostVersion;
    host_interface.log = LogMessage;
    last_palette_crc = 0;
    last_fog_crc = 0;
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
    asset_cache.Clear();
    unloadPlugin();
}

static FlycastCullMode MapCullMode(u32 pvrCullMode) {
    switch (pvrCullMode) {
        case 0: return FLYCAST_CULL_NONE;
        case 1: return FLYCAST_CULL_BACK;
        case 2: return FLYCAST_CULL_FRONT;
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
    if (!vtable) return;

    ta_parse(ctx, true);

    // 1. Palette Update
    uint32_t palette_hash = 0;
    for (int i = 0; i < 1024; i++) palette_hash ^= palette32_ram[i] + i;
    if (palette_hash != last_palette_crc) {
        if (vtable->update_palette) vtable->update_palette(palette32_ram);
        last_palette_crc = palette_hash;
    }

    // 2. Fog Table Update
    uint32_t fog_hash = 0;
    const uint32_t* fog_ptr = (const uint32_t*)FOG_TABLE;
    for (int i = 0; i < 128; i++) fog_hash ^= fog_ptr[i] + i;
    if (fog_hash != last_fog_crc) {
        if (vtable->update_fog_table) vtable->update_fog_table(fog_ptr);
        last_fog_crc = fog_hash;
    }

    // Determine capabilities
    bool use_mega_batch = false;
    if (vtable->get_capabilities && vtable->process_mega_batch) {
        use_mega_batch = (vtable->get_capabilities() & FLYCAST_CAP_MEGA_BATCH) != 0;
    }

    auto processList = [&](const std::vector<PolyParam>& polys, FlycastListType listType) {
        if (polys.empty()) return;

        if (use_mega_batch) {
            mega_commands.clear();
            mega_commands.reserve(polys.size());

            for (const auto& poly : polys) {
                if (poly.count == 0) continue;

                FlycastDrawCommand cmd = {};
                cmd.index_offset = poly.first; // Direct offset into ctx->rend.idx
                cmd.index_count = poly.count;

                cmd.scissor_enable = true;
                cmd.scissor_x = ctx->rend.fb_X_CLIP.min;
                cmd.scissor_y = ctx->rend.fb_Y_CLIP.min;
                cmd.scissor_w = (int32_t)ctx->rend.fb_X_CLIP.max - (int32_t)ctx->rend.fb_X_CLIP.min + 1;
                cmd.scissor_h = (int32_t)ctx->rend.fb_Y_CLIP.max - (int32_t)ctx->rend.fb_Y_CLIP.min + 1;
                if (cmd.scissor_w < 0) cmd.scissor_w = 0;
                if (cmd.scissor_h < 0) cmd.scissor_h = 0;

                cmd.cull_mode = MapCullMode(poly.isp.CullMode);

                cmd.texture_handle = 0;
                if (poly.pcw.Texture && poly.texture) {
                    uint32_t vram_addr = poly.tcw.TexAddr << 3;
                    uint32_t width = 8u << poly.tsp.TexU;
                    uint32_t height = 8u << poly.tsp.TexV;
                    FlycastTexMode mode = (poly.tcw.PixelFmt == PixelPal8) ? FLYCAST_TEX_PAL8 : FLYCAST_TEX_NONE;
                    cmd.texture_handle = asset_cache.GetTexture(vram_addr, width, height, mode,
                                                                poly.texture->Updates, FrameCount, &vram[vram_addr]);
                }

                cmd.src_blend = MapBlendFactor(poly.tsp.SrcInstr);
                cmd.dst_blend = MapBlendFactor(poly.tsp.DstInstr);
                cmd.depth_func = MapDepthFunc(poly.isp.DepthMode);
                cmd.depth_write = !poly.isp.ZWriteDis;
                cmd.offset_enable = poly.pcw.Offset;

                cmd.fog_mode = config::Fog ? poly.tsp.FogCtrl : 2;
                cmd.fog_color = FOG_COL_RAM.full;
                cmd.fog_vertex_color = FOG_COL_VERT.full;
                cmd.fog_density = FOG_DENSITY.get();
                cmd.fog_clamp_min = ctx->rend.fog_clamp_min.full;
                cmd.fog_clamp_max = ctx->rend.fog_clamp_max.full;

                mega_commands.push_back(cmd);
            }

            if (!mega_commands.empty()) {
                PluginMegaBatch mega = {};
                // Zero copy: Direct pointers to the global TA buffers
                mega.vertices = reinterpret_cast<const PluginVertex*>(ctx->rend.verts.data());
                mega.vertex_count = ctx->rend.verts.size();
                mega.indices = ctx->rend.idx.data();
                mega.index_count = ctx->rend.idx.size();
                mega.commands = mega_commands.data();
                mega.command_count = mega_commands.size();
                mega.list_type = listType;

                vtable->process_mega_batch(&mega);
            }

        } else if (vtable->process) {
            // Legacy Path (Draw-Call per Batch)
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

                data.cull_mode = MapCullMode(poly.isp.CullMode);

                data.texture_handle = 0;
                if (poly.pcw.Texture && poly.texture) {
                    uint32_t vram_addr = poly.tcw.TexAddr << 3;
                    uint32_t width = 8u << poly.tsp.TexU;
                    uint32_t height = 8u << poly.tsp.TexV;
                    FlycastTexMode mode = (poly.tcw.PixelFmt == PixelPal8) ? FLYCAST_TEX_PAL8 : FLYCAST_TEX_NONE;
                    data.texture_handle = asset_cache.GetTexture(vram_addr, width, height, mode,
                                                                poly.texture->Updates, FrameCount, &vram[vram_addr]);
                }

                data.src_blend = MapBlendFactor(poly.tsp.SrcInstr);
                data.dst_blend = MapBlendFactor(poly.tsp.DstInstr);
                data.depth_func = MapDepthFunc(poly.isp.DepthMode);
                data.depth_write = !poly.isp.ZWriteDis;
                data.offset_enable = poly.pcw.Offset;

                data.fog_mode = config::Fog ? poly.tsp.FogCtrl : 2;
                data.fog_color = FOG_COL_RAM.full;
                data.fog_vertex_color = FOG_COL_VERT.full;
                data.fog_density = FOG_DENSITY.get();
                data.fog_clamp_min = ctx->rend.fog_clamp_min.full;
                data.fog_clamp_max = ctx->rend.fog_clamp_max.full;
                data.list_type = listType;

                vtable->process(&data);
            }
        }
    };

    processList(ctx->rend.global_param_op, FLYCAST_LIST_OPAQUE);
    processList(ctx->rend.global_param_pt, FLYCAST_LIST_PUNCH_THROUGH);
    processList(ctx->rend.global_param_tr, FLYCAST_LIST_TRANSLUCENT);
}

bool HostRenderer::Render() {
    checkResize();
    if (vtable && vtable->render) return vtable->render();
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
    if (FrameCount % 120 == 0) asset_cache.CollectGarbage(FrameCount);
    if (vtable && vtable->present) return vtable->present();
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
        ERROR_LOG(RENDERER, "Invalid plugin API version. Expected %d, got %d", FLYCAST_PLUGIN_API_VERSION, vtable ? vtable->api_version : 0);
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