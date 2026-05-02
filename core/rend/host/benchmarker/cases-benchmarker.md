
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
