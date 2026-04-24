# Shader Architecture and Constants Documentation (Vulkan)

This documentation provides an exhaustive overview of the shader architecture in the Flycast Vulkan renderer, including hashing mechanisms, GLSL macro mappings, and all injected constants.

## 1. Hashing Overview

Flycast uses several levels of hashing to optimize rendering performance by caching expensive objects:

1.  **Pipeline Hash (64-bit)**: The most high-level hash. Used by `PipelineManager::GetPipeline` to select the entire Vulkan pipeline state (shaders, blending, depth/stencil, rasterization).
2.  **Fragment Shader Hash (32-bit)**: Used by `ShaderManager::GetFragmentShader` to compile and cache SPIR-V fragment modules. Multiple pipelines can share the same fragment shader if their fragment-specific states are identical.
3.  **Vertex Shader Hash (32-bit)**: Used by `ShaderManager::GetVertexShader` to compile and cache SPIR-V vertex modules.

---

## 2. Pipeline and Polygon Hashing (`shader poly`)

Used to select the full Vulkan pipeline.

**Reference File:** [pipeline.h](file:///mnt/data/projects/core/rend/vulkan/pipeline.h) (Method `PipelineManager::hash`)

### Exhaustive Pipeline Hash Bit Mapping (u64)

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

## 3. Fragment Shader Hashing (`shader frag`)

Used to select/compile the SPIR-V fragment module.

**Reference File:** [shaders.h](file:///mnt/data/projects/core/rend/vulkan/shaders.h) (Method `FragmentShaderParams::hash`)

### Exhaustive Fragment Hash Bit Mapping (u32)

| Bit | C++ Property | GLSL Macro Define | Description |
| :--- | :--- | :--- | :--- |
| 0 | `alphaTest` | `cp_AlphaTest` | Alpha test (Punch Through) |
| 1 | `insideClipTest` | `pp_ClipInside` | Internal clip test (User Clip) |
| 2 | `useAlpha` | `pp_UseAlpha` | Use alpha component in blending |
| 3 | `texture` | `pp_Texture` | Texturing enabled |
| 4 | `ignoreTexAlpha` | `pp_IgnoreTexA` | Ignore alpha from texture sampler |
| 5-6 | `shaderInstr` | `pp_ShadInstr` | Shader instruction (Decal, Modulate, etc.) |
| 7 | `offset` | `pp_Offset` | Offset color enabled |
| 8-9 | `fog` | `pp_FogCtrl` | Fog control mode |
| 10 | `gouraud` | `pp_Gouraud` | Gouraud shading enabled |
| 11 | `bumpmap` | `pp_BumpMap` | Bump mapping enabled (Naomi 2) |
| 12 | `clamping` | `ColorClamping` | Color clamping enabled |
| 13 | `trilinear` | `pp_TriLinear` | Trilinear filtering enabled |
| 14-15 | `palette` | `pp_Palette` | Palette mode (0: None, 1: 4bpp, 2: 8bpp) |
| 16 | `divPosZ` | `DIV_POS_Z` | Position W/Z division flag |
| 17 | `dithering` | `DITHERING` | Dithering enabled |
| 18 | `showDepth` | `ShowDepth` | Debug: Display depth buffer |
| 19 | `isTranslucent` | `IS_TRANSLUCENT` | Translucent list flag |
| 20 | `showNormals` | `ShowNormals` | Debug: Display normales |
| 21 | `gbuffer` | `GBUFFER` | Render to G-Buffer (Deferred) |
| 22 | `enableSSAO` | `EnableSSAO` | SSAO calculation enabled |
| 23 | `showSSAO` | `ShowSSAO` | Debug: Display SSAO buffer |
| 24 | `showMaterial` | `ShowMaterial` | Debug: Display Material IDs |

---

## 4. Injected Constants (Uniforms & Push Constants)

### A. Push Constants (48 bytes)
Injected at every draw call. Fragment Shader only.

| Struct Field | Type | Description |
| :--- | :--- | :--- |
| `pushConstants.isHUD` | `float` | HUD Routing Flag. 1.0 = HUD, 0.0 = 3D. |
| `pushConstants.clipTest` | `vec4` | Clipping coordinates (minX, minY, maxX, maxY). |
| `pushConstants.trilinearAlpha` | `float` | Alpha coefficient for trilinear filtering. |
| `pushConstants.palette_index` | `float` | Base offset for color palette (LUT) access. |
| `pushConstants.velocity` | `vec2` | Per-poly velocity vector for Motion Blur. |

### B. Uniform Buffers (UBO)

#### Set 0, Binding 1: `FragmentShaderUniforms` (as `uniformBuffer`)
- `uniformBuffer.colorClampMin/Max`: Bounds for color clamping.
- `uniformBuffer.sp_FOG_COL_RAM`: Fog color (from RAM).
- `uniformBuffer.sp_FOG_COL_VERT`: Fog color (from Vertex).
- `uniformBuffer.ditherDivisor`: Divisor for dithering.
- `uniformBuffer.cp_AlphaTestValue`: Reference value for Alpha Test.
- `uniformBuffer.sp_FOG_DENSITY`: Fog density constant.

---

## 5. Developer Guide: Adding New Parameters

### How to add a new Macro Bit (Shader Variation)
1.  **Modify `shaders.h`**:
    -   Add a new `bool` or `int` field to `FragmentShaderParams` (or `VertexShaderParams`).
    -   Update the `hash()` function to include the new field at an unused bit position.
2.  **Modify `shaders.cpp`**:
    -   In `compileShader`, add `src.addConstant("MY_NEW_MACRO", (int)params.myField);`.
3.  **Update Pipelines**:
    -   In `pipeline.cpp`, find `CreatePipeline` and ensure the new field is correctly initialized in the `params` struct passed to `shaderManager->GetFragmentShader`.
4.  **GLSL Usage**:
    -   In the `.frag` or `.vert` file, use `#if MY_NEW_MACRO == 1 ... #endif`.

### How to add a new Push Constant
1.  **Modify `pipeline.h`**:
    -   Update `vk::PushConstantRange` in `PipelineManager::Init` (increase the size, must be multiple of 4).
2.  **Modify `vulkan_top.frag`**:
    -   Add the variable in `layout(push_constant) uniform pushBlock`.
    -   **Important**: Follow `std430` alignment rules (e.g., `vec4` must start at 16-byte boundaries).
3.  **Update C++ side**:
    -   In `drawer.cpp`, update the `pushConstants` array/struct used in `cmdBuffer.pushConstants(...)` calls.

### How to add a new Uniform (UBO)
1.  **Modify `shaders.h`**:
    -   Add the field to `FragmentShaderUniforms` or `VertexShaderUniforms`.
    -   **Important**: Follow `std140` alignment rules (16-byte padding for vectors).
2.  **Modify `vulkan_top.frag`**:
    -   Add the field to the corresponding `uniform` block.
3.  **Populate the data**:
    -   In `drawer.h`, update `BaseDrawer::MakeFragmentUniforms` to fill the new field with data from the emulation state.

---

## Key Files Reference
- [shaders.h](file:///mnt/data/projects/core/rend/vulkan/shaders.h): Uniform structures and hashing logic.
- [shaders.cpp](file:///mnt/data/projects/core/rend/vulkan/shaders.cpp): Mapping params to GLSL macros.
- [pipeline.h](file:///mnt/data/projects/core/rend/vulkan/pipeline.h): Pipeline hashing logic.
- [vulkan_top.frag](file:///mnt/data/projects/core/rend/vulkan/shaders/vulkan_top.frag): GLSL declarations.
- [vulkan_main.frag](file:///mnt/data/projects/core/rend/vulkan/shaders/vulkan_main.frag): Main logic.