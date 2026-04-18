#if GBUFFER == 1
layout (location = GBUFFER_ALBEDO_INDEX) out vec4 FragColor;
layout (location = GBUFFER_NORMAL_INDEX) out vec4 NormalColor;
layout (location = GBUFFER_MATERIAL_INDEX) out uint MaterialColor;
layout (location = GBUFFER_MOTION_INDEX) out vec2 MotionColor;
layout (location = GBUFFER_HUD_INDEX) out vec4 HUDColor;
#else
layout (location = 0) out vec4 FragColor;
#define gl_FragColor FragColor
#endif

layout (std140, set = 0, binding = 1) uniform FragmentShaderUniforms
{
	vec4 colorClampMin;
	vec4 colorClampMax;
	vec4 sp_FOG_COL_RAM;
	vec4 sp_FOG_COL_VERT;
	vec4 ditherDivisor;
	float cp_AlphaTestValue;
	float sp_FOG_DENSITY;
} uniformBuffer;

layout (push_constant) uniform pushBlock
{
	float isHUD;
	float _pad0, _pad1, _pad2;  // std430: vec4 must be at offset 16 (12 bytes padding after float)
	vec4 clipTest;
	float trilinearAlpha;
	float palette_index;
	vec2 velocity;
} pushConstants;

#if pp_Texture == 1
layout (set = 1, binding = 0) uniform sampler2D tex;
#endif
#if pp_FogCtrl != 2
layout (set = 0, binding = 2) uniform sampler2D fog_table;
#endif
#if pp_Palette != 0
layout (set = 0, binding = 3) uniform sampler2D palette;
#endif

// Vertex input
layout (location = 0) INTERPOLATION in highp vec4 vtx_base;
layout (location = 1) INTERPOLATION in highp vec4 vtx_offs;
layout (location = 2) in highp vec3 vtx_uv;
layout (location = 3) in highp vec3 vtx_pos;
layout (location = 4) in highp vec3 vtx_normal;

