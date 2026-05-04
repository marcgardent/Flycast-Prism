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
*   **Benchmarker Action**: Shaders should consume colors as `.rgba` directly. No component swizzling is required.

## 2. Texture System
Textures processed by the host follow the `opengl::pvrTexInfo` conversion tables.

*   **Pixel Packing**: All 32-bit converted textures (VQ, Twiddled, or Strided) are packed into **RGBA8888**.
*   **Palettes (CLUT)**: The 1024-entry palette RAM is normalized into a 32-bit **RGBA** buffer (`palette32_ram`).
*   **Benchmarker Action**: Sampled texture colors will have Red in the first component (`.r`) and Blue in the third (`.b`).

## 3. Texture Coordinates (UV)
Flycast exports UV coordinates in their **binned PVR-native** format, which means they are **pre-multiplied by depth (1/W)**.

*   **Perspective Correction**: To achieve perspective-correct texture mapping in a screen-space rasterizer, the plugin MUST:
    1.  Interpolate the provided `u`, `v` (which are $U/W, V/W$) and `z` (which is $1/W$) linearly.
    2.  In the fragment shader, calculate the final coordinates: `final_u = u / z` and `final_v = v / z`.
    3.  **IMPORTANT**: Do NOT multiply UVs by `z` in the vertex shader, as they are already pre-multiplied.
*   **Orientation**:
    *   `U = 0.0` (Left), `U = 1.0` (Right)
    *   `V = 0.0` (**TOP**), `V = 1.0` (**BOTTOM**)
*   **`isOpenGL` Implication**: While standard OpenGL textures are often Y-bottom-up, Flycast does **not** flip the V coordinate in the `PluginVertex` buffer.
*   **Benchmarker Action**:
    *   If using a PVR-style texture upload (Top-Down), use UVs as-is (after the division by `z`).
    *   If using a standard OpenGL Y-up coordinate system, apply `v = 1.0 - v` in the fragment shader **after** the division by `z`.

## 4. Geometric Positions (X, Y, Z)
*   **Screen Space**: Coordinates are provided in absolute pixels (e.g., 0-640 for X, 0-480 for Y).
*   **Y-Axis**: `Y = 0` is at the **TOP** of the screen.
*   **Depth (Z)**: Native PVR depth (usually `1/W` for perspective-correct interpolation).
*   **Precision**: No "half-pixel" offsets (typical of DirectX 9) are applied.

---

### Summary Table for Shader Implementation

| Attribute | `isOpenGL` Standard | Implementation Note |
| :--- | :--- | :--- |
| **Vertex Color** | **RGBA8** | `layout(location = 1) in vec4 aColor;` |
| **Texture Sample** | **RGBA8** | `texture(tex, v_uv / v_z).rgba` |
| **UV Orientation** | **V=0 at TOP** | Matches VRAM layout. |
| **Winding Order** | **Triangle Strip** | Follow PVR parity rules. |
| **Normals** | **Standard Float** | (Naomi 2 only) |
