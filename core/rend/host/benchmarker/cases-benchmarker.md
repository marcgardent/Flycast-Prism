
# Validation Plan and Benchmark (Dummy Dreamcast)

To ensure the native backend pipeline perfectly respects PowerVR2 specifications, a "Dummy Dreamcast" (dataset generator) must submit these specific scenarios to the engine.

| Test ID | Category | Rendering Scenario (Dummy Input) | Expected Result (Backend Validation) |
| :--- | :--- | :--- | :--- |
| **GEO-01** | **Geometry** | A simple square (2 triangles) sent to the *Opaque* list. | Displays a solid square. Validates basic VBO/IBO creation and rendering. |
| **GEO-02** | **Culling** | Three overlapping triangles with different winding orders, configured with Front, Back, and None culling. | Only triangles matching the requested culling state are visible. |
| **GEO-03** | **Clipping** | A full-screen polygon with a 320x240 Scissor zone in the center. | Only the center of the screen displays the polygon. The rest is the clear color. |
| **SHD-01** | **Interpolation** | A triangle with Red/Green/Blue vertices. Mode: *Gouraud*. | Perfectly smooth color gradient across the triangle. |
| **TEX-01** | **Palette Lookup** | An indexed sprite (8BPP) with a rainbow palette buffer. | The image must use the exact colors from the palette buffer (validating the 1D texture/UBO lookup). |
| **TRN-01** | **Punch-Through** | Wire mesh texture covering the screen (100% opaque and 0% alpha pixels). | The mesh "holes" show the clear color. Edges are sharp, with no blending blur. Validates the `discard` shader logic. |
| **OIT-01** | **Transparency Sort**| 50 overlapping translucent planes sent in random order. | Validates the CPU back-to-front sorting algorithm or the backend's native OIT implementation. |
| **MOD-01** | **Opaque Modifier** | An opaque gray plane intersected by an invisible triangular modifier volume. | The exact geometric intersection area is darkened. Validates the Stencil Buffer masking implementation. |
| **MOD-02** | **Translucent Mod** | A transparent window intersected by a modifier volume. | Darkening only applies to the window, without affecting the opaque wall behind it. Validates separated stencil passes. |
| **SPE-01** | **Table/Vertex Fog** | Distant 3D corridor with Z/W fog, and explicit Offset Color Alpha fog. | The backend fragment shader correctly calculates and applies fog density based on the provided mode. |



## 3. Limit Testing & Edge Cases (Stress Benchmark)

This suite is designed to push the emulator and the native API backend to their absolute limits, triggering floating-point errors, memory leaks, or algorithm instability.

| Test ID | Category | Extreme Scenario (Stress Input) | Expected Result (Edge Case Validation) |
| :--- | :--- | :--- | :--- |
| **EXT-GEO-01**| **Degenerate Polys** | Submission of 10,000 triangles with an area of 0 (collinear vertices or same coordinates). | The backend must safely cull or discard them without crashing or causing divide-by-zero errors in the rasterizer. |
| **EXT-GEO-02**| **Massive Bounds** | A triangle where vertices are placed at coordinates +/- 1,000,000, far beyond the viewport. | Validates that floating-point precision loss doesn't break the Scissor/Viewport clipping logic. |
| **EXT-TEX-01**| **1x1 / NPOT** | Textures measuring exactly 1x1 pixel, and weird Non-Power-Of-Two (e.g., 3x7 pixels). | UV mapping and sampler states must not fetch out-of-bounds memory or cause API validation errors. |
| **EXT-TEX-02**| **Palette Thrash** | Modifying the Palette RAM buffer 100 times *during* the same render pass, between draw calls. | Validates that the backend correctly versions the UBOs/1D Textures and doesn't stall the GPU pipeline. |
| **EXT-TRN-01**| **Threshold Edge** | Punch-Through texture where the Alpha value is *exactly* equal to the hardware cutoff threshold. | Validates shader precision (`>=` vs `>`). It must exactly match the original Dreamcast silicium behavior. |
| **EXT-TRN-02**| **Z-Fighting Sort**| 500 overlapping Translucent polygons submitted with the *exact same* Z-depth value. | The CPU sorting algorithm must remain stable (Stable Sort) and not flicker randomly from frame to frame. |
| **EXT-MOD-01**| **Near-Plane Clip**| A Modifier Volume that extends *behind* the camera's near-clipping plane. | Validates the Stencil logic. If the geometry is clipped by the near-plane, standard stencil shadows fail (requires Z-fail / Carmack's Reverse algorithm validation). |
| **EXT-SPE-01**| **Zero-Depth Fog** | Table Fog enabled where the Near Fog plane and Far Fog plane are exactly the same value. | The shader must not produce NaN (Not a Number) or black pixels due to a division by zero in the fog density formula. |
