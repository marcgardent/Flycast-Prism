# G-Buffer Architecture

This document provides a technical specification for the G-Buffer implementation in Flycast, specifically focusing on the deferred shading pipeline, post-processing chain, and the structure of the exported OpenEXR files.

## Overview

The G-Buffer is part of a **modern Deferred Shading pipeline** in the Vulkan renderer. It captures essential scene information in a single geometry pass. All G-Buffer attachments are treated as **read-only** after the geometry pass; no post-process writes back into them (non-destructive principle).

The G-Buffer can be exported to an OpenEXR file by pressing **ALT+9** during emulation.

## Rendering Pipeline

The pipeline follows a strict 5-phase execution order within `GBufferVulkanRenderer::Present()`:

```
┌─────────────────────────────────────────────────────────────────────┐
│  Phase A: Geometry Pass (G-Buffer Fill)                            │
│  → Writes: Albedo, Normals, MaterialID, Motion, HUD, Depth        │
│  → After this pass, ALL G-Buffer attachments become READ-ONLY      │
└────────────────────────┬────────────────────────────────────────────┘
                         ▼
┌─────────────────────────────────────────────────────────────────────┐
│  Phase B: SSAOPass (Screen-Space Ambient Occlusion)                │
│  → Reads: Depth, Normals                                           │
│  → Writes: ssaoTex (R8Unorm) — isolated, non-destructive           │
└────────────────────────┬────────────────────────────────────────────┘
                         ▼
┌─────────────────────────────────────────────────────────────────────┐
│  Phase C: CompositePass (Deferred Lighting)                        │
│  → Reads: Albedo, Normals, Depth, MaterialID, Motion, ssaoTex, HUD│
│  → Writes: Accumulation Buffer (R16G16B16A16Sfloat — HDR)          │
│  → Formula: color = Albedo.rgb * SSAO                              │
└────────────────────────┬────────────────────────────────────────────┘
                         ▼
┌─────────────────────────────────────────────────────────────────────┐
│  Phase D: DoFPass (Depth of Field — Post-Processing)               │
│  → Reads: Accumulation Buffer, Depth                               │
│  → Writes: Accumulation Buffer (ping-pong via temp buffer)         │
│  → Operates on the lit HDR image, not raw Albedo                   │
└────────────────────────┬────────────────────────────────────────────┘
                         ▼
┌─────────────────────────────────────────────────────────────────────┐
│  Phase E: FinalPass (Tonemapping + HUD Overlay)                    │
│  → Reads: Accumulation Buffer (post-processed), HUD Buffer         │
│  → Writes: Final output (R8G8B8A8Unorm → Swapchain)                │
│  → HUD is composited here (unaffected by DoF)                      │
│  → Tonemapping (Reinhard) can be enabled here                      │
└─────────────────────────────────────────────────────────────────────┘
```

### Key Design Principles

1. **G-Buffer Immutability**: Once the geometry pass completes, G-Buffer attachments (Albedo, Normals, Depth, etc.) are 100% read-only (`ShaderReadOnlyOptimal`). No post-process writes into them.
2. **Dedicated Accumulation Buffer**: Lighting and global effects are computed and stored in a dedicated HDR buffer (`R16G16B16A16Sfloat`), separate from the raw G-Buffer data.
3. **Strict Pass Separation**: Each pass has well-defined inputs and a single output. No implicit side effects.
4. **Non-Destructive HUD**: The HUD is composited only in the final pass, ensuring it is never affected by DoF or other camera effects.

## Passes Implementation

### SSAOPass
- **Shader**: `vulkan_ssao.frag`
- **Input**: Depth attachment, Normal attachment
- **Output**: `ssaoTex` (R8Unorm) — standalone AO factor
- **Note**: Writes exclusively to its own buffer. The old destructive multiply-on-Albedo behavior has been removed.

### CompositePass (Lighting Pass)
- **Shader**: `vulkan_gbuffer_composite.frag`
- **Input**: All G-Buffer attachments + ssaoTex
- **Output**: Accumulation Buffer (`R16G16B16A16Sfloat`)
- **Debug modes**: viewMode push constant selects between Final (0), Albedo (1), Normals (2), Depth (3), Material (4), Motion (5), SSAO (6), HUD (7)

### DoFPass (Depth of Field)
- **Shader**: `vulkan_dof.frag`
- **Input**: Accumulation Buffer, Depth attachment
- **Output**: Accumulation Buffer (via blit from temp buffer)
- **Note**: Operates on the lit HDR image, not raw Albedo. Internal format is `R16G16B16A16Sfloat` to preserve HDR precision.

### FinalPass (Tonemapping + HUD Overlay)
- **Shader**: `vulkan_gbuffer_final.frag`
- **Input**: Accumulation Buffer, HUD attachment
- **Output**: Final image (`R8G8B8A8Unorm`) ready for swapchain presentation
- **Note**: Only executed for `viewMode == 0` (Final) or `viewMode == 7` (HUD debug). Other debug views bypass this pass and present the Accumulation Buffer directly.

## OpenEXR Export Structure

The exported `.exr` file is a multi-channel image containing **15 channels**. All channels are stored as **16-bit half-precision floats** (FP16), even if their source data was integer or 32-bit float.

### Channel Mapping

| EXR Channel Name | Source Attachment | Description | Data Range |
| :--- | :--- | :--- | :--- |
| `Albedo.R/G/B` | Attachment 0 (R8G8B8A8) | Diffuse Color (RGB) | [0.0, 1.0] |
| `Normal.X/Y/Z` | Attachment 1 (R16G16B16A16F) | World/View Space Normal (XYZ) | [-1.0, 1.0] |
| `Depth.Z` | Depth Attachment | Linearized/Raw Depth | [0.0, 1.0] |
| `Material.ID` | Attachment 2 (R8Uint) | Encoded Material Properties | [0.0, 1.0] (ID/255) |
| `SSAO.AO` | SSAO Pass (R8Unorm) | Ambient Occlusion Factor | [0.0, 1.0] |
| `Motion.X/Y` | Attachment 3 (R16G16F) | Per-object Velocity Vector | [-1.0, 1.0] |
| `HUD.R/G/B/A` | Attachment 4 (R8G8B8A8) | Isolated HUD/UI Elements | [0.0, 1.0] |

## Buffer Details

### 1. Albedo (`Albedo.R/G/B`)
*   **Format**: `eR8G8B8A8Unorm`.
*   **Content**: The base color of the surface. If the pixel belongs to a HUD element (DepthMode >= 6), it is cleared to black in this buffer and moved to the HUD attachment.
*   **Normalization**: Divided by 255.0.

### 2. Normals (`Normal.X/Y/Z`)
*   **Format**: `eR16G16B16A16Sfloat`.
*   **Content**: Surface normals. Generated using cross-products of position derivatives if vertex normals are absent.

### 3. Depth (`Depth.Z`)
*   **Format**: `eD32Sfloat` or `eD24UnormS8Uint`.
*   **Content**: Raw depth value. Linearization depends on the Dreamcast's 1/W or Z coordinate modes.

### 4. Material ID (`Material.ID`)
*   **Format**: `eR8Uint`.
*   **Normalization**: Stored as `matID / 255.0` in the EXR.
*   **Reconstruction**: `uint8_t id = round(Material.ID * 255.0)`.

#### Bitmask Encoding (8 bits)
| Bits | Name | Description | Values |
| :--- | :--- | :--- | :--- |
| **7-5** | `list_type` | PVR List Type | 0: Opaque, 2: Translucent, 4: Punch-Through |
| **4** | `has_texture` | Texture presence | 0: No, 1: Yes |
| **3** | `is_gouraud` | Shading mode | 0: Flat, 1: Gouraud |
| **2** | `has_bumpmap` | Bump mapping | 0: No, 1: Yes |
| **1** | `fog_active` | Fog status | 0: Disabled, 1: Enabled |
| **0** | `palette_active`| Palette status | 0: No, 1: Yes |

### 5. SSAO (`SSAO.AO`)
*   **Format**: `eR8Unorm`.
*   **Content**: Pre-calculated occlusion factor. `1.0` = No occlusion.

### 6. Motion (`Motion.X/Y`)
*   **Format**: `eR16G16Sfloat`.
*   **Content**: Screen-space velocity vector. Encoded as `velocity * 0.5 + 0.5` in the shader.
*   **Range**: Centered at 0.5. `(0.5, 0.5)` means zero motion.

### 7. HUD (`HUD.R/G/B/A`)
*   **Format**: `eR8G8B8A8Unorm`.
*   **Content**: Pixels identified as HUD (DepthMode >= 6). This attachment is composited over the final scene in `FinalPass` (not in the G-Buffer pass).

### 8. Accumulation Buffer (Internal)
*   **Format**: `eR16G16B16A16Sfloat` (HDR).
*   **Content**: Result of the Deferred Lighting pass. Contains lit scene color with SSAO applied. This buffer is NOT exported to EXR; it is an intermediate rendering target.

## Technical Notes

*   **File Naming**: `gbuffer_YYYYMMDD_HHMMSS.exr` in the screenshots directory.
*   **Hotkeys**:
    *   **ALT+1/2/3/4/5/7/8**: Toggle various debug views (Depth, Normals, SSAO, Motion, Material, Albedo, HUD).
    *   **ALT+7**: Show pure Albedo buffer (No HUD, No post-effects).
    *   **ALT+8**: Show HUD attachment alone.
    *   **ALT+0**: Return to final composite view.
    *   **ALT+9**: Export current G-Buffer to EXR.
*   **Integration**: The G-Buffer layout is defined in `core/rend/vulkan/gbuffer/gbuffer_constants.h`.

## Key Source Files

| File | Role |
| :--- | :--- |
| `core/rend/vulkan/gbuffer/gbuffer_renderer.cpp` | Pipeline orchestration, all passes, `Present()` |
| `core/rend/vulkan/gbuffer/gbuffer_constants.h` | G-Buffer attachment index constants |
| `core/rend/vulkan/shaders/vulkan_gbuffer_composite.frag` | Deferred Lighting / Debug views shader |
| `core/rend/vulkan/shaders/vulkan_gbuffer_final.frag` | Tonemapping + HUD overlay shader |
| `core/rend/vulkan/shaders/vulkan_ssao.frag` | SSAO computation shader |
| `core/rend/vulkan/shaders/vulkan_dof.frag` | Depth of Field shader |
| `core/rend/vulkan/shaders/vulkan_main.frag` | Geometry pass (G-Buffer fill) |
| `core/rend/vulkan/drawer.h` | `ScreenDrawer` class (G-Buffer framebuffer management) |
| `core/rend/vulkan/shaders.h` / `shaders.cpp` | Shader compilation and management |
| `resources/resources.cmake` | cmrc shader registration (required for embedding) |

## Changelog

- **2026-04-17**: Refonte complète vers un pipeline Deferred Shading non-destructif. Ajout de l'Accumulation Buffer HDR, de la FinalPass, isolation du SSAO, correction du DoF (opère sur l'image éclairée), séparation HUD en passe finale.
- **2026-04-16**: Séparation du HUD dans un buffer G-Buffer dédié (Attachment 4).
- **2026-04-17**: Ajout des buffers Motion (Attachment 3) et HUD (Attachment 4), export OpenEXR 15 canaux.
