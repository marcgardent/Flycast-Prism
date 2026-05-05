
# Validation Plan and Benchmark (PowerVR2 Specifications)

This list summarizes the active test cases used to validate rendering backend conformity with PowerVR2 (Dreamcast) hardware.

| Test ID | Category | Rendering Scenario | Expected Result |
| :--- | :--- | :--- | :--- |
| **GEO-01** | **Geometry** | A simple square (2 triangles) in the *Opaque* list. | Displays a solid square. Validates VBO/IBO and basic draw. |
| **GEO-02** | **Culling** | Overlapping triangles with Z-Inverse and CW/CCW winding. | Validates Y-Down axis, Z-Greater depth, and Backface culling. |
| **GEO-03** | **Clipping** | Full-screen quad with a 320x240 Scissor zone. | Only the center displays the polygon. Validates Scissor logic. |
| **GEO-04** | **Stability** | Multiple distinct batches in the same area. | No "spider web" artifacts (verifies tile counter clearing). |
| **GEO-05** | **Strips** | Single Triangle Strip (4 vertices). | One solid square. Validates Even/Odd strip winding. |
| **GEO-06** | **Robustness** | Triangles with NaN and Infinity coordinates. | No crash/corruption. Invalid tris must be culled. |
| **GEO-08** | **Culling** | CCW Triangle with Front-Face culling mode. | Triangle must be invisible. Validates Front-Face mode. |
| **SHD-01** | **Shading** | Triangle with Red/Green/Blue vertices (Gouraud). | Perfectly smooth color gradient across the triangle. |
| **SHD-02** | **Interpolation**| Deeply tilted quad with vertex colors. | No visible diagonal seam (verifies perspective barycentrics). |
| **TEX-01** | **Texturing** | Indexed sprite (8BPP) with a 256-color palette. | The image must use colors from the Hardware Palette RAM. |
| **TEX-02** | **Texturing** | Quadrant texture (R,G,B,Y) to verify UV axes. | Validates PVR-native UV orientation (V=0 at TOP). |
| **TEX-03** | **Perspective** | Checkerboard quad deeply tilted into the Z-axis. | Perfectly straight grid lines. Validates 1/W interpolation. |
| **TRN-01** | **Blending** | Translucent blue quad over opaque red background. | Correct alpha blending (SrcAlpha/InvSrcAlpha). |
| **TRN-02** | **Alpha Test** | Fully invisible triangle (Alpha=0) in foreground. | No "hole" in background. Validates Alpha-Discard logic. |
| **OIT-01** | **Transparency**| 3 overlapping quads sent in Front-to-Back order. | Correct blending regardless of submission order (OIT). |
| **SPE-01** | **Specular** | Triangle with "Offset Color" additive secondary pass. | Primary color + Specular offset correctly summed. |
| **SPE-02** | **Fog** | 3D corridor with Table Fog (128-entry LUT). | Colors fade into gray fog based on depth lookup. |
| **GC-01** | **System** | Texture used for 10 frames then abandoned. | GPU resource is destroyed after 120 frames of inactivity. |

---
*For technical implementation details and vertex-level schemas, see [VALIDATION_DOC.md](VALIDATION_DOC.md).*
