#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif


#define FLYCAST_PLUGIN_API_VERSION 12


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
 * Blending factors
 */
typedef enum {
    FLYCAST_BLEND_ZERO            = 0,
    FLYCAST_BLEND_ONE             = 1,
    FLYCAST_BLEND_OTHER_COLOR     = 2,
    FLYCAST_BLEND_INV_OTHER_COLOR = 3,
    FLYCAST_BLEND_SRC_ALPHA       = 4,
    FLYCAST_BLEND_INV_SRC_ALPHA   = 5,
    FLYCAST_BLEND_DST_ALPHA       = 6,
    FLYCAST_BLEND_INV_DST_ALPHA   = 7
} FlycastBlendFactor;

/**
 * Depth comparison functions
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
 * PVR2 List Types
 */
typedef enum {
    FLYCAST_LIST_OPAQUE           = 0,
    FLYCAST_LIST_PUNCH_THROUGH    = 1,
    FLYCAST_LIST_TRANSLUCENT      = 2
} FlycastListType;

typedef enum {
    FLYCAST_INDEX_UINT16 = 0,
    FLYCAST_INDEX_UINT32 = 1
} FlycastIndexFormat;

/**
 * Legacy Structure (Retained for backwards compatibility)
 */
typedef struct {
    const PluginVertex* vertices;
    size_t vertex_count;
    const uint16_t* indices;
    size_t index_count;


    bool scissor_enable;
    int32_t scissor_x;
    int32_t scissor_y;
    int32_t scissor_w;
    int32_t scissor_h;

    FlycastCullMode cull_mode;
    uint32_t        texture_handle;

    FlycastBlendFactor src_blend;
    FlycastBlendFactor dst_blend;
    FlycastDepthFunc   depth_func;
    bool               depth_write;
    bool               offset_enable;

    uint32_t           fog_mode;
    uint32_t           fog_color;
    uint32_t           fog_vertex_color;
    float              fog_density;
    uint32_t           fog_clamp_min;
    uint32_t           fog_clamp_max;

    FlycastListType    list_type;
} PluginGeometryData;

// ============================================================================
// V12 MEGA-BATCHING & SUB-ALLOCATION
// ============================================================================

typedef enum {
    FLYCAST_CAP_NONE = 0,
    FLYCAST_CAP_MEGA_BATCH = (1 << 0) // Plugin requests full Mega-Batches with DrawCommands
} PluginCapabilities;

/**
 * A single draw command (Lot) inside a Mega-Batch.
 */
typedef struct {
    uint32_t index_offset; // Offset within the global MegaBatch indices array
    uint32_t index_count;  // Number of indices to draw

    // Material & State
    uint32_t texture_handle;
    FlycastBlendFactor src_blend;
    FlycastBlendFactor dst_blend;
    FlycastDepthFunc depth_func;
    bool depth_write;
    FlycastCullMode cull_mode;
    bool offset_enable;

    // Scissor
    bool scissor_enable;
    int32_t scissor_x;
    int32_t scissor_y;
    int32_t scissor_w;
    int32_t scissor_h;

    // Fog
    uint32_t fog_mode;
    uint32_t fog_color;
    uint32_t fog_vertex_color;
    float fog_density;
    uint32_t fog_clamp_min;
    uint32_t fog_clamp_max;
} FlycastDrawCommand;

/**
 * A full list of geometry (e.g., all Opaque polygons for the frame).
 * Zero-copy: points directly to the emulator's global vertex/index buffers.
 */
typedef struct {
    const PluginVertex* vertices;
    size_t vertex_count;

    const void* indices;
    size_t index_count;
    FlycastIndexFormat index_format;

    const FlycastDrawCommand* commands;
    size_t command_count;

    FlycastListType list_type;
} PluginMegaBatch;

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
    const char* (*get_game_id)(FlycastHostHandle host);
    const char* (*get_host_name)(FlycastHostHandle host);
    const char* (*get_host_version)(FlycastHostHandle host);
    void (*log)(FlycastHostHandle host, FlycastLogLevel level, const char* message);
} FlycastHostInterface;

// ============================================================================
// PLUGIN EXPORTED INTERFACE
// ============================================================================

typedef struct {
    uint32_t struct_size;
    uint32_t api_version;

    const char* name;
    const char* version;

    bool (*init)(FlycastHostHandle host, const FlycastWindowHandle* window, const FlycastHostInterface* host_if);
    void (*term)(void);
    void (*resize)(uint32_t width, uint32_t height);

    // Rendering capabilities (Added v12)
    uint32_t (*get_capabilities)(void);

    // Legacy render (fallback if MEGA_BATCH is not supported)
    void (*process)(const PluginGeometryData* data);

    // Mega-Batch render (v12)
    void (*process_mega_batch)(const PluginMegaBatch* batch);

    bool (*render)(void);
    void (*render_framebuffer)(const PluginFramebufferInfo* info);
    bool (*present)(void);

    // State Management
    void (*update_palette)(const uint32_t* palette_data);
    void (*update_fog_table)(const uint32_t* fog_table_data);

    // Texture Management
    uint32_t (*create_texture)(uint32_t width, uint32_t height, FlycastTexMode mode);
    void (*update_texture)(uint32_t handle, const uint8_t* data);
    void (*destroy_texture)(uint32_t handle);

} FlycastPluginVTable;

#if defined(_WIN32)
  #define PLUGIN_EXPORT __declspec(dllexport)
#else
  #define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

PLUGIN_EXPORT const FlycastPluginVTable* flycast_plugin_get_vtable(void);

#ifdef __cplusplus
}
#endif
