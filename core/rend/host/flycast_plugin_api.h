#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// OPAQUE TYPES & HOST INTERFACE
// ============================================================================

/**
 * Opaque handle representing the Flycast engine host.
 */
typedef struct FlycastHost_s* FlycastHostHandle;

// ============================================================================
// WINDOW HANDLING (Agnostic Backend)
// ============================================================================

typedef enum {
    FLYCAST_OS_WINDOWS,
    FLYCAST_OS_X11,
    FLYCAST_OS_WAYLAND,
    FLYCAST_OS_MACOS,
    FLYCAST_OS_ANDROID
} FlycastOSType;

/**
 * Native window information for the plugin
 * to create its own graphics context (Vulkan, WGPU, etc.)
 */
typedef struct {
    FlycastOSType os_type;
    void* display; // X11 Display* or wl_display*
    void* window;  // HWND, X11 Window, wl_surface*, ANativeWindow*
} FlycastWindowHandle;

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/**
 * Simplified structure for a Vertex.
 * (Memory representation must match internal C++ Vertex)
 */
typedef struct {
    float x, y, z;
    uint8_t col[4];
    uint8_t spc[4];
    float u, v;
    uint8_t col1[4];
    uint8_t spc1[4];
    float u1, v1;
    float nx, ny, nz;
} PluginVertex;

/**
 * Container for geometry data sent during `Process()`.
 */
typedef struct {
    const PluginVertex* vertices;
    size_t vertex_count;

    const uint32_t* indices;
    size_t index_count;
} PluginGeometryData;

/**
 * Framebuffer information.
 */
typedef struct {
    uint32_t fb_r_sof1;
    uint32_t fb_r_sof2;
    uint32_t width;
    uint32_t height;
} PluginFramebufferInfo;

// ============================================================================
// PLUGIN EXPORTED INTERFACE
// ============================================================================

#define FLYCAST_PLUGIN_API_VERSION 1

/**
 * Function table that the Rust plugin MUST implement.
 */
typedef struct {
    uint32_t struct_size; // For API version checking
    uint32_t api_version;

    // The plugin receives window info to self-initialize
    bool (*init)(FlycastHostHandle host, const FlycastWindowHandle* window);
    void (*term)(void);

    // Equivalent to Renderer::Process
    void (*process)(const PluginGeometryData* data);

    // Equivalent to Renderer::Render
    bool (*render)(void);

    // Equivalent to Renderer::RenderFramebuffer
    void (*render_framebuffer)(const PluginFramebufferInfo* info);

    // Equivalent to SwapBuffers or vkQueuePresentKHR
    bool (*present)(void);

} FlycastPluginVTable;

/**
 * The ONLY function exported (symbol) by the dynamic plugin library (.so/.dll).
 * In Rust: #[no_mangle] pub extern "C" fn flycast_plugin_get_vtable() -> *const FlycastPluginVTable
 */
#if defined(_WIN32)
  #define PLUGIN_EXPORT __declspec(dllexport)
#else
  #define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

PLUGIN_EXPORT const FlycastPluginVTable* flycast_plugin_get_vtable(void);

#ifdef __cplusplus
}
#endif
