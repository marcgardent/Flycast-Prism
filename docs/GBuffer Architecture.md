# G-Buffer Architecture

This document provides a technical specification for the G-Buffer implementation in Flycast, specifically focusing on the structure and encoding of the exported OpenEXR files.

## Overview

The G-Buffer is part of the deferred rendering pipeline in the Vulkan renderer. It captures essential scene information in a single pass, which is then used for post-processing effects such as SSAO (Screen Space Ambient Occlusion), Depth of Field (DoF), and material visualization.

The G-Buffer can be exported to an OpenEXR file by pressing **ALT+9** during emulation.

## OpenEXR File Structure

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
*   **Content**: Pixels identified as HUD (DepthMode >= 6). This attachment is composited over the final scene in the `HUDCompositePass`.

## Technical Notes

*   **File Naming**: `gbuffer_YYYYMMDD_HHMMSS.exr` in the screenshots directory.
*   **Hotkeys**:
    *   **ALT+1/2/3/4/5/7/8**: Toggle various debug views (Depth, Normals, SSAO, Motion, Material, Albedo, HUD).
    *   **ALT+7**: Show pure Albedo buffer (No HUD, No post-effects).
    *   **ALT+8**: Show HUD attachment alone.
    *   **ALT+0**: Return to final composite view.
    *   **ALT+9**: Export current G-Buffer to EXR.
*   **Integration**: The G-Buffer layout is defined in `core/rend/vulkan/gbuffer/gbuffer_constants.h`.
