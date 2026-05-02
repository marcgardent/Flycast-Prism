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

typedef enum {
    FLYCAST_CULL_NONE  = 0,
    FLYCAST_CULL_FRONT = 1,
    FLYCAST_CULL_BACK  = 2
} FlycastCullMode;

/**
 * Texture mode for a draw batch.
 */
typedef enum {
    FLYCAST_TEX_NONE = 0, // No texture, use vertex color (Gouraud)
    FLYCAST_TEX_PAL8 = 1  // 8BPP indexed texture with 256-entry ARGB32 palette
} FlycastTexMode;

/**
 * Blending factors (Added in v7)
 * Based on PVR2 hardware modes.
 */
typedef enum {
    FLYCAST_BLEND_ZERO            = 0,
    FLYCAST_BLEND_ONE             = 1,
    FLYCAST_BLEND_OTHER_COLOR     = 2, // Source: Dest Color, Dest: Source Color
    FLYCAST_BLEND_INV_OTHER_COLOR = 3, // Source: 1 - Dest Color, Dest: 1 - Source Color
    FLYCAST_BLEND_SRC_ALPHA       = 4,
    FLYCAST_BLEND_INV_SRC_ALPHA   = 5,
    FLYCAST_BLEND_DST_ALPHA       = 6,
    FLYCAST_BLEND_INV_DST_ALPHA   = 7
} FlycastBlendFactor;

/**
 * Depth comparison functions (Added in v7)
 * Matches PVR2 depth modes.
 */
typedef enum {
    FLYCAST_DEPTH_NEVER    = 0,
    FLYCAST_DEPTH_LESS     = 1,
    FLYCAST_DEPTH_EQUAL    = 2,
    FLYCAST_DEPTH_LEQUAL   = 3,
    FLYCAST_DEPTH_GREATER  = 4,
    FLYCAST_DEPTH_NOTEQUAL = 5,
    FLYCAST_DEPTH_GEQUAL   = 6,
    FLYCAST_DEPTH_ALWAYS   = 7
} FlycastDepthFunc;

/**
 * Container for geometry data sent during `Process()`.
 */
typedef struct {
    const PluginVertex* vertices;
    size_t vertex_count;

    const uint32_t* indices;
    size_t index_count;

    // Scissor / Clipping state (Added in v4)
    bool scissor_enable;
    int32_t scissor_x; // Supports negative values for widescreen
    int32_t scissor_y;
    int32_t scissor_w;
    int32_t scissor_h;

    // Culling state (Added in v5)
    FlycastCullMode cull_mode;

    // Texture state (Added in v6)
    FlycastTexMode  tex_mode;    // Texture sampling mode
    uint32_t        tex_width;   // Texture width in pixels
    uint32_t        tex_height;  // Texture height in pixels
    const uint8_t*  tex_data;    // tex_width * tex_height bytes (8BPP palette indices)
    const uint32_t* palette;     // 256 ARGB32 entries (A=MSB, B=LSB)

    // Transparency state (Added in v7)
    FlycastBlendFactor src_blend;
    FlycastBlendFactor dst_blend;

    // Depth state (Added in v7)
    FlycastDepthFunc   depth_func;
    bool               depth_write;

    // Specular / Offset Color (Added in v8)
    bool               offset_enable;

    // Fog state (Added in v9)
    uint32_t           fog_mode;         // 0: Table, 1: Vertex, 2: None, 3: Table 2
    uint32_t           fog_color;        // ARGB8888
    uint32_t           fog_vertex_color; // ARGB8888
    float              fog_density;      // From FOG_DENSITY register
    uint32_t           fog_clamp_min;    // ARGB8888
    uint32_t           fog_clamp_max;    // ARGB8888
    const uint32_t*    fog_table;        // 128 entries (32-bit each)
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
// HOST INTERFACE (CALLBACKS)
// ============================================================================

typedef enum {
    FLYCAST_LOG_DEBUG = 0,
    FLYCAST_LOG_INFO  = 1,
    FLYCAST_LOG_WARN  = 2,
    FLYCAST_LOG_ERROR = 3
} FlycastLogLevel;

typedef struct {
    /**
     * Returns the current Game ID (e.g., "MK-51000").
     * Pointer valid until next call or plugin termination.
     */
    const char* (*get_game_id)(FlycastHostHandle host);

    /**
     * Returns the host name ("Flycast").
     */
    const char* (*get_host_name)(FlycastHostHandle host);

    /**
     * Returns the host version string.
     */
    const char* (*get_host_version)(FlycastHostHandle host);

    /**
     * Sends a log message to the host.
     */
    void (*log)(FlycastHostHandle host, FlycastLogLevel level, const char* message);

} FlycastHostInterface;

// ============================================================================
// PLUGIN EXPORTED INTERFACE
// ============================================================================

#define FLYCAST_PLUGIN_API_VERSION 9

/**
 * Function table that the Rust/C++ plugin MUST implement.
 */
typedef struct {
    uint32_t struct_size; // For API version checking
    uint32_t api_version;

    // Plugin identification (provided by plugin)
    const char* name;
    const char* version;

    // The plugin receives window info and host interface to self-initialize
    bool (*init)(FlycastHostHandle host, const FlycastWindowHandle* window, const FlycastHostInterface* host_if);
    void (*term)(void);

    /**
     * Called when the host window is resized.
     */
    void (*resize)(uint32_t width, uint32_t height);

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
