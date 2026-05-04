
## . Limit Testing & Edge Cases (Stress Benchmark)

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
