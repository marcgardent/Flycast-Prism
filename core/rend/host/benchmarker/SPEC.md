# Flycast Benchmarker Specification: `isOpenGL` Mode

This document outlines the technical specifications and data schemas for the Flycast Mega-Batch API (v14) when operating in `isOpenGL` mode (standard for Plugin Engines).

## 1. Color Schema (Vertex & Global)
The `isOpenGL` mode enforces a strict **RGBA8** byte order for all color data. Flycast normalizes all internal Dreamcast formats to this standard before exporting to the plugin.

*   **Memory Layout**: `[Offset 0: R, Offset 1: G, Offset 2: B, Offset 3: A]`
*   **Vertex Colors (`PluginVertex.col`)**:
    *   Unpacked from PVR native `0xAARRGGBB`.
    *   Red is mapped to index 0, Blue to index 2.
*   **Intensity Conversion**: If the hardware uses `Intensity` mode, the parser pre-calculates the final color:  
    `FinalColor = (GlobalFaceColor * VertexIntensity) / 256`.  
    The result is stored in the **RGBA8** schema.
*   **Gouraud Shading**: Unlike UVs, **vertex colors are NOT pre-multiplied by depth** in the vertex buffer. 
*   **Benchmarker Action**: Shaders should consume colors as `.rgba`. For perspective-correct Gouraud shading, the plugin may need to manually multiply colors by `z` (1/W) in the vertex shader and divide by `interpolated_z` in the fragment shader.

## 2. Texture System
Textures processed by the host follow the `opengl::pvrTexInfo` conversion tables.

*   **Pixel Packing**: All 32-bit converted textures (VQ, Twiddled, or Strided) are packed into **RGBA8888**.
*   **Palettes (CLUT)**: The 1024-entry palette RAM is normalized into a 32-bit **RGBA** buffer (`palette32_ram`).
*   **Benchmarker Action**: Sampled texture colors will have Red in the first component (`.r`) and Blue in the third (`.b`).

## 3. Texture Coordinates (UV)
Flycast exports UV coordinates in their **RAW PVR-native** format, which means they are **NOT pre-multiplied by depth (1/W)**.

*   **Perspective Correction**: To achieve perspective-correct texture mapping in a screen-space rasterizer, the plugin MUST:
    1.  **Vertex Shader**: Multiply the incoming `u`, `v` coordinates by the depth `z` (1/W) and pass the result as a `vec3` varying: `v_uv = vec3(u * z, v * z, z)`.
    2.  **Fragment Shader**: Calculate the final coordinates using projective sampling or manual division: `final_uv = v_uv.xy / v_uv.z`.
    3.  **Note**: This manual process is required because screen-space rasterization (typical of Mega-Batch processing) does not benefit from the GPU's automatic `W` division if `gl_Position.w` is fixed to 1.0.
*   **Orientation**:
    *   `U = 0.0` (Left), `U = 1.0` (Right)
    *   `V = 0.0` (**TOP**), `V = 1.0` (**BOTTOM**)
*   **`isOpenGL` Implication**: While standard OpenGL textures are often Y-bottom-up, Flycast does **not** flip the V coordinate in the `PluginVertex` buffer.
*   **Benchmarker Action**:
    *   If using a PVR-style texture upload (Top-Down), use UVs as-is (after perspective correction).
    *   If using a standard OpenGL Y-up coordinate system, apply `v = 1.0 - v` in the fragment shader **after** perspective correction.

## 4. Geometric Positions (X, Y, Z)
*   **Coordinate System**: Coordinates are provided in absolute pixels (0-640). `Y = 0` is at the **TOP**.
*   **Winding Convention**: In this Y-Down system, a **Clockwise (CW)** triangle has a **positive** cross-product area, while **Counter-Clockwise (CCW)** is **negative**.
*   **Depth (Z)**: Native PVR depth is exported as **`1/W`**. Higher values are closer.
*   **Precision**: No "half-pixel" offsets (typical of DirectX 9) are applied.

## 5. Geometric Topology & Indexing
The Mega-Batch API (v14) exports geometry in its native **Triangle Strip** format to preserve the original PVR submission order.

*   **Topology**: Each `FlycastDrawCommand` represents a single triangle strip starting at `vertex_offset` with `vertex_count` vertices.
*   **Unrolling Algorithm**: For plugins requiring `TriangleList` (e.g., WGPU, Vulkan), you MUST apply the **Even/Odd winding rule** during index buffer generation:
    *   **Even Index (`i % 2 == 0`)**: Triangle = `(i, i+1, i+2)`
    *   **Odd Index (`i % 2 != 0`)**: Triangle = `(i+1, i, i+2)` (Flipped to maintain front-face orientation).
*   **Degenerate Triangles**: The host may emit duplicate adjacent vertices to link discontinuous strips. These zero-area triangles (where any two indices are identical) should be discarded.

## 6. Face Culling
Culling is controlled by the 2-bit PVR CullMode found in the `ISP_TSP` instruction:

| PVR Mode | Value | Interpretation | `FlycastCullMode` |
| :--- | :--- | :--- | :--- |
| **None** | `0` | No culling. | `FLYCAST_CULL_NONE` |
| **Small** | `1` | Area < threshold. | `FLYCAST_CULL_NONE` (Safe Fallback) |
| **CCW** | `2` | Front-Face. | `FLYCAST_CULL_FRONT` |
| **CW** | `3` | Back-Face. | `FLYCAST_CULL_BACK` |

*   **Winding Inversion (`DCalcCtrl`)**: If the `DCalcCtrl` bit is set in the `ISP_TSP` word, the winding is inverted. The `FlycastDrawCommand`'s `cull_mode` already accounts for this by swapping `FRONT` and `BACK`.

## 7. Clipping (Scissor & User Clip)
The `FlycastDrawCommand` provides a pre-computed `scissor` rectangle.
*   **Global Clip**: Always applied to keep rendering within the valid framebuffer region.
*   **User Clip**: If `User_Clip` (PCW bits 22-23) is enabled (Mode 2: Inside), the host intersects the global clip with the `USER_CLIP` register coordinates.

---

### Summary Table for Shader Implementation

| Attribute | `isOpenGL` Standard | Implementation Note |
| :--- | :--- | :--- |
| **Vertex Color** | **RGBA8** | `v_color * v_z / interpolated_z` (Persp. Correction) |
| **Texture Sample** | **RGBA8** | `(v_uv * v_z) / interpolated_z` (Persp. Correction) |
| **Topology** | **Triangle Strip** | Use Even/Odd rule for Triangle List conversion. |
| **Winding Order** | **Alternating** | Flip Odd triangles to maintain Front-Face. |
| **Cull Mode** | **PVR 2-bit** | 0=None, 1=Small, 2=Front(CCW), 3=Back(CW). |
| **UV Orientation** | **V=0 at TOP** | Matches VRAM layout. |
| **Depth (Z)** | **1/W** | Higher values are closer (Z-Greater). |
