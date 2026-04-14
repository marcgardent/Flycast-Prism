R"(
#define PI 3.1415926

#if pp_FogCtrl != 2 || pp_TwoVolumes == 1
float fog_mode2(float w)
{
	float z = clamp(
#if DIV_POS_Z == 1
					uniformBuffer.sp_FOG_DENSITY / w
#else
					uniformBuffer.sp_FOG_DENSITY * w
#endif
													, 1.0, 255.9999);
	float exp = floor(log2(z));
	float m = z * 16.0 / pow(2.0, exp) - 16.0;
	float idx = floor(m) + exp * 16.0 + 0.5;
	vec4 fog_coef = texture(fog_table, vec2(idx / 128.0, 0.75 - (m - floor(m)) / 2.0));
	return fog_coef.r;
}
#endif

vec4 colorClamp(vec4 col)
{
#if ColorClamping == 1
	return clamp(col, uniformBuffer.colorClampMin, uniformBuffer.colorClampMax);
#else
	return col;
#endif
}

#if pp_Palette != 0

vec4 getPaletteEntry(float colIdx)
{
	vec2 c = vec2(colIdx * 255.0 / 1023.0 + pushConstants.palette_index, 0.5);
	return texture(palette, c);
}

#endif

#if pp_Palette == 1

vec4 palettePixel(sampler2D tex, vec3 coords)
{
#if DIV_POS_Z == 1
	return getPaletteEntry(texture(tex, coords.xy).r);
#else
	return getPaletteEntry(textureProj(tex, coords).r);
#endif
}

#elif pp_Palette == 2

vec4 palettePixelBilinear(sampler2D tex, vec3 coords)
{
#if DIV_POS_Z == 0
	coords.xy /= coords.z;
#endif
	vec2 texSize = vec2(textureSize(tex, 0));
	vec2 pixCoord = coords.xy * texSize - 0.5;			// coordinates of top left pixel
	vec2 originPixCoord = floor(pixCoord);

	vec2 sampleUV = (originPixCoord + 0.5) / texSize;	// UV coordinates of center of top left pixel

    // Sample from all surrounding texels
    vec4 c00 = getPaletteEntry(texture(tex, sampleUV).r);
    vec4 c01 = getPaletteEntry(textureOffset(tex, sampleUV, ivec2(0, 1)).r);
    vec4 c11 = getPaletteEntry(textureOffset(tex, sampleUV, ivec2(1, 1)).r);
    vec4 c10 = getPaletteEntry(textureOffset(tex, sampleUV, ivec2(1, 0)).r);

	vec2 weight = pixCoord - originPixCoord;

    // Bi-linear mixing
    vec4 temp0 = mix(c00, c10, weight.x);
    vec4 temp1 = mix(c01, c11, weight.x);
    return mix(temp0, temp1, weight.y);
}

#endif
)"
