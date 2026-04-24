#pragma once

#define GBUFFER_ALBEDO_INDEX   0
#define GBUFFER_NORMAL_INDEX   1
#define GBUFFER_MATERIAL_INDEX 2
#define GBUFFER_MOTION_INDEX   3
#define GBUFFER_HUD_INDEX      4
#define GBUFFER_TEXHASH_INDEX  5
#define GBUFFER_POLYDATA_INDEX 6

// G-Buffer formats used in GBufferVulkanRenderer::Init
// Albedo (0)       : eR8G8B8A8Unorm
// Normals (1)      : eR16G16B16A16Sfloat
// Material ID (2)  : eR8Uint
// Motion (3)       : eR16G16Sfloat
// HUD (4)          : eR8G8B8A8Unorm
// TextureHash (5)  : eR32Uint
// PolyData (6)     : eR32G32B32A32Sfloat

#define STENCIL_MASK_SHADOW 0x80

// Material ID bits (8 bits total)
#define MATERIAL_BIT_PRESENT     0x80 // Bit 7: Presence (1 for geometry)
#define MATERIAL_MASK_LIST_TYPE  0x70 // Bits 6-4: List type
#define MATERIAL_BIT_TEXTURE     0x08 // Bit 3: Texture enabled
#define MATERIAL_BIT_GOURAUD     0x04 // Bit 2: Gouraud shading
#define MATERIAL_BIT_BUMPMAP     0x02 // Bit 1: Bump mapping
#define MATERIAL_BIT_FOG         0x01 // Bit 0: Fog/Palette

// List types for MATERIAL_MASK_LIST_TYPE (shifted right by 4)
#define LIST_TYPE_OPAQUE         0
#define LIST_TYPE_TRANSLUCENT    2
#define LIST_TYPE_PUNCH_THROUGH  4

