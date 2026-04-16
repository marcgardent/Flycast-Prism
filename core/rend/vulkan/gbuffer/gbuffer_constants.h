#pragma once

#define GBUFFER_ALBEDO_INDEX   0
#define GBUFFER_NORMAL_INDEX   1
#define GBUFFER_MATERIAL_INDEX 2
#define GBUFFER_MOTION_INDEX   3
#define GBUFFER_HUD_INDEX      4

// G-Buffer formats used in GBufferVulkanRenderer::Init
// Albedo (0)       : eR8G8B8A8Unorm
// Normals (1)      : eR16G16B16A16Sfloat
// Material ID (2)  : eR8Uint
// Motion (3)       : eR16G16Sfloat
// HUD (4)          : eR8G8B8A8Unorm
