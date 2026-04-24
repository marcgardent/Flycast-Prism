# VULKAN RENDERING ARCHITECTURE - FLYCAST "NEXT-GEN" (G-BUFFER & STENCIL)

## 1. DATA STRUCTURE (G-BUFFER & ATTACHMENTS)
Recommended Global Format: **16-bit or 32-bit Float** (EXR compatible) for lossless data transfer to native formats.

### [Attachment 0]: ALBEDO + SHADOW MASK
* **RGB**: Raw color (unlit).
* **A**: Shadow Mask (0.0 = Black, 1.0 = White). Allows for independent shadow blurring.

### [Attachment 1]: NORMALS + MATERIAL ID
* **RGB**: Normal vectors (World Space or View Space).
* **A**: MaterialID (Injected via Push Constant).

### [Attachment 2]: DEPTH + STENCIL
* **Depth**: W-Buffer depth (0.0 = Infinite/Sky, 1.0 = Near).
* **Stencil**: Semantic Masking (Critical for optimization).
    * `0x00`: SKY / VOID (Ignored by SSAO and shadows).
    * `0x01`: OPAQUE OBJECTS (Full processing).
    * `0x02`: TRANSLUCENT OBJECTS (Ignored by SSAO, processed in Forward).

---

## 2. RENDERING STEPS (PHASES)

### PHASE A: GEOMETRY PASS (OPAQUES)
* **Output**: G-Buffer (Albedo, Normals, Depth, Stencil).
* **CPU Optimization**: Sort polygons by Pipeline Hash (Macros).
* **Dynamic Injection**:
    * Push Constants -> MaterialID.
    * Stencil Reference -> Tag ID (0x01 for opaques).
* *Note*: Sky (Z=0) is marked as Stencil=0x00.

### PHASE B: SSAO PASS (AMBIENT OCCLUSION)
* **Input**: Depth, Normals, Stencil.
* **Stencil Filter**: "EQUAL 0x01" test.
* **Result**: SSAO does NOT run on the sky. **30%+ performance gain**.

### PHASE C: SHADOW BILATERAL BLUR
* **Input**: Albedo.Alpha (Shadow Mask), Depth, Normals.
* **Action**: Cross Bilateral Filter (Edge-Aware).
* **Logic**: Blur is weighted by depth and normals.
* **Result**: Shadows become soft but never bleed onto background objects.

### PHASE D: COMPOSITE PASS (LIGHTING)
* **Formula**: `Color = (Albedo * Ambient * SSAO) + (Albedo * DirectLight * ShadowMask)`.

### PHASE E: TRANSLUCENCY PASS
* **Mode**: Forward Rendering directly onto the Accumulation Buffer.
* **Stencil Filter**: "EQUAL 0x02" test.

---

## 3. KEY OPTIMIZATIONS
1. **Push Constants**: Removed "MaterialID" from Pipeline Hash. Calls dropped from ~600 to ~20.
2. **Stencil**: Used for perfect sky isolation and zero overdraw on HUD.
3. **Shadow Mask**: Independent blurring without affecting Albedo textures.
    