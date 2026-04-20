# Shader Architecture and Constants Documentation (Vulkan)

This documentation provides an exhaustive overview of the shader architecture in the Flycast Vulkan renderer, including pipeline hashing mechanisms, GLSL macro mappings, and all injected constants (Uniforms and Push Constants).

## 1. Pipeline and Polygon Hashing (`shader poly`)

The `PipelineManager` uses a **64-bit hash** to uniquely identify a rendering state configuration. This hash determines whether a new Vulkan pipeline needs to be created or if an existing one can be reused.

**Reference File:** [pipeline.h](file:///mnt/data/projects/core/rend/vulkan/pipeline.h) (Method `PipelineManager::hash`)

### Exhaustive Pipeline Hash Bit Mapping (u64) & GLSL Macros

| Bits | C++ Source (PolyParam) | GLSL Macro Define | Description |
| :--- | :--- | :--- | :--- |
| 0 | `pp->pcw.Gouraud` | `pp_Gouraud` | Gouraud Shading enabled |
| 1 | `pp->pcw.Offset` | `pp_Offset` | Offset Color (Specular) enabled |
| 2 | `pp->pcw.Texture` | `pp_Texture` | Texturing enabled |
| 3 | `pp->pcw.Shadow` | - | Shadow Mode enabled (Vulkan Stencil State) |
| 4 | `pp->tileclip >> 28 == 3` | `pp_ClipInside` | Alpha Blend Enable / Internal Clip Test |
| 5-6 | `listType >> 1` | - | List Type (Internal Pipeline State) |
| 7-8 | `pp->tsp.ShadInstr` | `pp_ShadInstr` | Shading Instruction (Decal, Modulate, etc.) |
| 9 | `ignoreTexAlpha` | `pp_IgnoreTexA` | True if `IgnoreTexA` set or format `Pixel565` |
| 10 | `pp->tsp.UseAlpha` | `pp_UseAlpha` | Use alpha for blending |
| 11 | `pp->tsp.ColorClamp` | `ColorClamping` | Color Clamping enabled |
| 12-13 | `pp->tsp.FogCtrl` | `pp_FogCtrl` | Fog Control mode (or 2 if disabled) |
| 14-16 | `pp->tsp.SrcInstr` | - | Source Blend Factor (Vulkan Blend State) |
| 17-19 | `pp->tsp.DstInstr` | - | Destination Blend Factor (Vulkan Blend State) |
| 20 | `pp->isp.ZWriteDis` | - | Z-Buffer write disabled (Vulkan Depth State) |
| 21-22 | `pp->isp.CullMode` | - | Culling Mode (Vulkan Rasterizer State) |
| 23-25 | `pp->isp.DepthMode` | - | Depth Comparison (Vulkan Depth State) |
| 26 | `sortTriangles` | - | Per-strip triangle sorting enabled |
| 27-28 | `gpuPalette` | `pp_Palette` | GPU Palette Index (4bpp, 8bpp) |
| 29 | `pp->isNaomi2()` | - | Naomi 2 specific rendering |
| 30 | `NativeDepth` | `DIV_POS_Z` | Native Depth Interpolation (non-Naomi 2) |
| 31 | `BumpMap` | `pp_BumpMap` | Bump Map pixel format (Naomi 2) |
| 32 | `dithering` | `DITHERING` | Dithering enabled |
| 33 | `config::ShowDepth` | `ShowDepth` | Debug Mode: Show Depth |
| 34 | `DepthTrans` | `IS_TRANSLUCENT` | Show Depth/Normals for transparent objects |
| 35 | `config::ShowNormals` | `ShowNormals` | Debug Mode: Show Normals |
| 36 | `config::ShowSSAO` | `ShowSSAO` | Debug Mode: Show SSAO |
| 37 | `config::EnableSSAO` | `EnableSSAO` | SSAO enabled |
| 38 | `config::ShowMaterial` | `ShowMaterial` | Debug Mode: Show Material IDs |
| 39 | **`isHUD`** | - | **HUD Routing Flag (via Push Constant)** |

---

## 2. Fragment Shader Hashing (`shader frag`)

Used by the `ShaderManager` to compile and cache SPIR-V modules based on the PowerVR fragment state.

**Reference File:** [shaders.h](file:///mnt/data/projects/core/rend/vulkan/shaders.h) (Method `FragmentShaderParams::hash`)

### Fragment Shader Hash Bit Mapping (u32) & GLSL Macros

- **Bits 0-4**: `alphaTest` (`cp_AlphaTest`), `insideClipTest` (`pp_ClipInside`), `useAlpha` (`pp_UseAlpha`), `texture` (`pp_Texture`), `ignoreTexAlpha` (`pp_IgnoreTexA`)
- **Bits 5-6**: `shaderInstr` (`pp_ShadInstr`)
- **Bits 7-9**: `offset` (`pp_Offset`), `fog` (`pp_FogCtrl`)
- **Bits 10-13**: `gouraud` (`pp_Gouraud`), `bumpmap` (`pp_BumpMap`), `clamping` (`ColorClamping`), `trilinear` (`pp_TriLinear`)
- **Bits 14-15**: `palette` (`pp_Palette`)
- **Bit 16**: `divPosZ` (`DIV_POS_Z`)
- **Bit 17**: `dithering` (`DITHERING`)
- **Bits 18-20**: `showDepth` (`ShowDepth`), `isTranslucent` (`IS_TRANSLUCENT`), `showNormals` (`ShowNormals`)
- **Bits 21-24**: `gbuffer` (`GBUFFER`), `enableSSAO` (`EnableSSAO`), `showSSAO` (`ShowSSAO`), `showMaterial` (`ShowMaterial`)

---

## 3. Injected Constants (Uniforms & Push Constants)

### A. Push Constants (48 bytes)
Injected at every draw call (`DrawPoly`). Accessible in the **Fragment Shader** only.
**GLSL Declaration:** [vulkan_top.frag](file:///mnt/data/projects/core/rend/vulkan/shaders/vulkan_top.frag)

| Struct Field | Type | Description |
| :--- | :--- | :--- |
| `pushConstants.isHUD` | `float` | HUD Routing Flag (Whitelist-based). 1.0 = HUD, 0.0 = 3D. |
| `pushConstants.clipTest` | `vec4` | Clipping coordinates (minX, minY, maxX, maxY). |
| `pushConstants.trilinearAlpha` | `float` | Alpha coefficient for trilinear filtering interpolation. |
| `pushConstants.palette_index` | `float` | Base offset for color palette (LUT) access. |
| `pushConstants.velocity` | `vec2` | Per-poly velocity vector for Motion Blur. |

### B. Uniform Buffers (UBO)
Defined in [shaders.h](file:///mnt/data/projects/core/rend/vulkan/shaders.h).

#### Set 0, Binding 0: `VertexShaderUniforms`
- `uniformBuffer.ndcMat`: Matrix for projection to Normalized Device Coordinates.

#### Set 0, Binding 1: `FragmentShaderUniforms` (as `uniformBuffer`)
- `uniformBuffer.colorClampMin/Max`: Bounds for color clamping.
- `uniformBuffer.sp_FOG_COL_RAM`: Fog color (from RAM).
- `uniformBuffer.sp_FOG_COL_VERT`: Fog color (from Vertex).
- `uniformBuffer.ditherDivisor`: Divisor for dithering calculations.
- `uniformBuffer.cp_AlphaTestValue`: Reference value for Alpha Test (Punch Through).
- `uniformBuffer.sp_FOG_DENSITY`: Fog density constant.

#### Set 1, Binding 2: `N2VertexShaderUniforms` (Naomi 2 only)
- `mvMat`, `normalMat`, `projMat`, `envMapping[2]`, `bumpMapping`, `polyNumber`, `glossCoef[2]`, `constantColor[2]`.

---

## 4. Shader Routing (HUD Separation)

The main fragment shader uses the `isHUD` push constant to decide which G-Buffer attachment to write to.

**Reference File:** [vulkan_main.frag](file:///mnt/data/projects/core/rend/vulkan/shaders/vulkan_main.frag)

```glsl
#if GBUFFER == 1
    if (pushConstants.isHUD > 0.0) {
        HUDColor = color;      // Writes to dedicated HUD attachment
        FragColor = vec4(0.0); // Discards output in main Albedo buffer
    } else {
        HUDColor = vec4(0.0);
        FragColor = color;     // Regular 3D rendering
    }
#endif
```

---

## Key Files Reference
- [shaders.h](file:///mnt/data/projects/core/rend/vulkan/shaders.h): Uniform structures and shader hashing logic.
- [shaders.cpp](file:///mnt/data/projects/core/rend/vulkan/shaders.cpp): Mapping between C++ params and GLSL `#define` macros.
- [pipeline.h](file:///mnt/data/projects/core/rend/vulkan/pipeline.h): Pipeline hashing logic and layout definition.
- [vulkan_top.frag](file:///mnt/data/projects/core/rend/vulkan/shaders/vulkan_top.frag): GLSL declarations (Bindings, Push Constants).
- [vulkan_main.frag](file:///mnt/data/projects/core/rend/vulkan/shaders/vulkan_main.frag): Core rendering logic implementation.
- [drawer.cpp](file:///mnt/data/projects/core/rend/vulkan/drawer.cpp): C++ side constant upload and draw dispatch.
- [ta_structs.h](file:///mnt/data/projects/core/hw/pvr/ta_structs.h): Hardware PowerVR structures (PCW, TSP, ISP).