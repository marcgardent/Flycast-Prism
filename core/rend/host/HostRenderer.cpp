#include "HostRenderer.h"
#include "log/LogManager.h"
#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
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
        ERROR_LOG(RENDERER, "Failed to load plugin renderer.");
        return false;
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
#if defined(_WIN32)
    plugin_handle = LoadLibrary("flycast_dummy_plugin.dll");
    if (!plugin_handle) {
        ERROR_LOG(RENDERER, "Could not load flycast_dummy_plugin.dll");
        return false;
    }
    auto get_vtable = (const FlycastPluginVTable* (*)())GetProcAddress((HMODULE)plugin_handle, "flycast_plugin_get_vtable");
#else
    plugin_handle = dlopen("libflycast_dummy_plugin.so", RTLD_LAZY | RTLD_LOCAL);
    if (!plugin_handle) {
        ERROR_LOG(RENDERER, "Could not load libflycast_dummy_plugin.so: %s", dlerror());
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
    // TODO: Fetch SDL_Window and use SDL_GetWindowWMInfo to populate the handle
    // For now we set it to Wayland/X11 or whatever
#if defined(_WIN32)
    handle.os_type = FLYCAST_OS_WINDOWS;
#elif defined(__APPLE__)
    handle.os_type = FLYCAST_OS_MACOS;
#elif defined(__ANDROID__)
    handle.os_type = FLYCAST_OS_ANDROID;
#else
    handle.os_type = FLYCAST_OS_X11; // Fallback
#endif
    return handle;
}

} // namespace rend
