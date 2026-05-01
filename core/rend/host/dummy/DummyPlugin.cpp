#include "../flycast_plugin_api.h"
#include <iostream>

static bool dummy_init(FlycastHostHandle host, const FlycastWindowHandle* window) {
    std::cout << "[DummyPlugin] init() called. OS Type: " << window->os_type << std::endl;
    return true;
}

static void dummy_term() {
    std::cout << "[DummyPlugin] term() called." << std::endl;
}

static void dummy_process(const PluginGeometryData* data) {
    if (data) {
        std::cout << "[DummyPlugin] process() called. " 
                  << data->vertex_count << " vertices, "
                  << data->index_count << " indices." << std::endl;
    }
}

static bool dummy_render() {
    return true;
}

static void dummy_render_framebuffer(const PluginFramebufferInfo* info) {
    if (info) {
        std::cout << "[DummyPlugin] render_framebuffer() " 
                  << info->width << "x" << info->height << std::endl;
    }
}

static bool dummy_present() {
    return true;
}

static const FlycastPluginVTable vtable = {
    sizeof(FlycastPluginVTable),
    FLYCAST_PLUGIN_API_VERSION,
    dummy_init,
    dummy_term,
    dummy_process,
    dummy_render,
    dummy_render_framebuffer,
    dummy_present
};

PLUGIN_EXPORT const FlycastPluginVTable* flycast_plugin_get_vtable(void) {
    return &vtable;
}
