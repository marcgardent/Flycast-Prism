# Flycast Benchmarker: Validation & Test Suite

This document provides a detailed breakdown of the diagnostic tests implemented in the Flycast Benchmarker. These tests are designed to verify that a rendering plugin correctly implements the PowerVR2 (Dreamcast) specifications.

## 1. Geometry (GEO)

| ID | Name | Description | Expected Result |
| :--- | :--- | :--- | :--- |
| **GEO-01** | **Simple Square** | Basic geometry rendering: a simple opaque square made of two triangles. | A solid white square (Z=0.5) centered on the screen on a dark background. |
| **GEO-02** | **PVR Conformity** | Validates Y-Down axis, Z-Inverse depth (`Greater` function) and PVR culling (Back = CW). | Three triangles: Green in front of Red (Left), and Blue alone (Right). Yellow must be culled. |
| **GEO-03** | **Clipping (Scissor)** | Scissor rectangle test. Clips rendering to a specific 2D rectangular region. | A white rectangle in the center (320x240), clipping a full-screen quad. |
| **GEO-04** | **Multi-Batch Leak** | Validates that 'tile_counters' are cleared between batches. Prevents "spider web" corruption. | A large red background with a smaller green triangle strictly in the center. |
| **GEO-07** | **Overdraw Logic** | Draws two overlapping translucent triangles within the SAME batch. | A purple intersection. If bugged, the blue triangle will have a 'hole' where the red one is. |

## 2. Shading & Interpolation (SHD)

| ID | Name | Description | Expected Result |
| :--- | :--- | :--- | :--- |
| **SHD-01** | **Interpolation (Gouraud)** | Vertex color interpolation test. A single triangle with Red, Green, Blue vertices. | A large triangle (Z=0.5) with a perfectly smooth color gradient. |
| **SHD-02** | **Perspective Interpolation**| Large quad tilted deeply into the Z-axis. Tests for perspective-correct barycentric interpolation. | A smooth gradient across the entire quad. NO visible diagonal line cutting the quad in half. |

## 3. Texturing (TEX)

| ID | Name | Description | Expected Result |
| :--- | :--- | :--- | :--- |
| **TEX-01** | **Palette Lookup (8BPP)** | 8BPP indexed texture mapping. Texels are indices into a 1024-entry RGBA8 palette. | A full-screen rainbow gradient (Z=0.5) using an 8BPP indexed texture. |
| **TEX-02** | **UV Orientation** | Verifies that V=0 is TOP and V=1 is BOTTOM (standard PVR layout). | Red (Top-Left), Green (Top-Right), Blue (Bottom-Left), Yellow (Bottom-Right). |
| **TEX-03** | **Perspective Mapping** | Deeply tilted quad with a high-contrast 8x8 checkerboard. | Perfectly straight grid lines receding into the distance. No "kinks" or bending at the diagonal. |

## 4. Transparency & Blending (TRN)

| ID | Name | Description | Expected Result |
| :--- | :--- | :--- | :--- |
| **TRN-01** | **Alpha Blending** | Standard Alpha Blending (SrcAlpha / InvSrcAlpha). Overlaps Blue (50% Alpha) on Red. | A purple rectangle in the center where quads overlap. |
| **TRN-02** | **Punch-Through Bug** | Invisible foreground triangle (Alpha=0) in front of an opaque background. | A solid red triangle. The invisible triangle must NOT update depth or discard background pixels. |
| **OIT-01** | **Order Independent Trans.**| Three translucent quads (R, G, B) submitted in **Front-to-Back** order. | Quads should blend correctly. If sorting is missing, only the front quad will be visible. |

## 5. Special Effects (SPE)

| ID | Name | Description | Expected Result |
| :--- | :--- | :--- | :--- |
| **SPE-01** | **Specular (Offset Color)** | Validates additive blending of the secondary "Offset Color" onto the primary vertex color. | Blue (Black+Blue offset), Yellow (Green+Red offset), and Green (No offset). |
| **SPE-02** | **Fog Corridor** | Table Fog rendering test using a 128-entry lookup table. Simulates a deep corridor. | A 3D corridor where colors fade into gray fog based on depth (Z=1.0 to Z=0.05). |

## 6. System & Performance (GC)

| ID | Name | Description | Expected Result |
| :--- | :--- | :--- | :--- |
| **GC-01** | **Garbage Collection** | Verifies that GPU textures are destroyed after 120 frames of inactivity. | Textured triangle disappears after 10 frames. Logs should show "Destroying texture" around frame 130. |

---

## 7. Roadmap (Upcoming Tests)

The following tests are planned but not yet implemented in the current C++ test suite:

- **MOD-01 (Opaque Modifier)**: Stencil-based masking for opaque volumes.
- **MOD-02 (Translucent Modifier)**: Separate stencil pass for translucent volume clipping.
- **EXT-GEO (Degenerate Polys)**: Stability test for zero-area triangles.
- **EXT-TEX (Palette Thrash)**: Rapid palette updates between draw calls in a single frame.
