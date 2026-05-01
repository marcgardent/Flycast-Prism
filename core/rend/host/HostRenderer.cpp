#include "HostRenderer.h"
#include "log/LogManager.h"
#include "cfg/option.h"
#include "hw/pvr/ta.h"
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

Renderer* rend_HostRenderer() {
    return new rend::HostRenderer();
}

namespace rend {

HostRenderer::HostRenderer() {
}

HostRenderer::~HostRenderer() {
    Term();
}

bool HostRenderer::Init() {
    if (!loadPlugin()) {
        ERROR_LOG(RENDERER, "Failed to load plugin renderer. Running as norend fallback.");
        return true;
    }

    FlycastWindowHandle windowHandle = getWindowHandle();

    if (vtable && vtable->init) {
        return vtable->init(reinterpret_cast<FlycastHostHandle>(this), &windowHandle);
    }
    return false;
}

void HostRenderer::Term() {
    if (vtable && vtable->term) {
        vtable->term();
    }
    unloadPlugin();
}

void HostRenderer::Process(TA_context *ctx) {
    if (!vtable || !vtable->process) return;

    ta_parse(ctx, true);

    PluginGeometryData data;
    data.vertices = reinterpret_cast<const PluginVertex*>(ctx->rend.verts.data());
    data.vertex_count = ctx->rend.verts.size();
    
    data.indices = ctx->rend.idx.data();
    data.index_count = ctx->rend.idx.size();

    vtable->process(&data);
}

bool HostRenderer::Render() {
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
    // TODO: Calculate real resolution from info
    pluginInfo.width = 640;
    pluginInfo.height = 480;

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

} // namespace rend
