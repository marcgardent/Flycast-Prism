precision highp float;
precision highp int;

void main()
{
	#if pp_ClipInside == 1
		if (gl_FragCoord.x >= pushConstants.clipTest.x && gl_FragCoord.x <= pushConstants.clipTest.z
				&& gl_FragCoord.y >= pushConstants.clipTest.y && gl_FragCoord.y <= pushConstants.clipTest.w)
			discard;
	#endif

	highp vec4 color = vtx_base;
	highp vec4 offset = vtx_offs;
	#if pp_Gouraud == 1 && DIV_POS_Z != 1
		color /= vtx_uv.z;
		offset /= vtx_uv.z;
	#endif

	#if pp_UseAlpha == 0
		color.a = 1.0;
	#endif

	#if pp_FogCtrl == 3
		color = vec4(uniformBuffer.sp_FOG_COL_RAM.rgb, fog_mode2(vtx_uv.z));
	#endif

	#if pp_Texture == 1
	{
		#if pp_Palette == 0
			#if DIV_POS_Z == 1
				vec4 texcol = texture(tex, vtx_uv.xy);
			#else
				vec4 texcol = textureProj(tex, vtx_uv);
			#endif
		#else
			#if pp_Palette == 1
				vec4 texcol = palettePixel(tex, vtx_uv);
			#else
				vec4 texcol = palettePixelBilinear(tex, vtx_uv);
			#endif
		#endif

		#if pp_BumpMap == 1
			float s = PI / 2.0 * (texcol.a * 15.0 * 16.0 + texcol.r * 15.0) / 255.0;
			float r = 2.0 * PI * (texcol.g * 15.0 * 16.0 + texcol.b * 15.0) / 255.0;
			texcol.a = clamp(offset.a + offset.r * sin(s) + offset.g * cos(s) * cos(r - 2.0 * PI * offset.b), 0.0, 1.0);
			texcol.rgb = vec3(1.0, 1.0, 1.0);
		#else
			#if pp_IgnoreTexA == 1
				texcol.a = 1.0;
			#endif
		#endif

		#if pp_ShadInstr == 0
			color = texcol;
		#elif pp_ShadInstr == 1
			color.rgb *= texcol.rgb; color.a = texcol.a;
		#elif pp_ShadInstr == 2
			color.rgb = mix(color.rgb, texcol.rgb, texcol.a);
		#elif pp_ShadInstr == 3
			color *= texcol;
		#endif

		#if pp_Offset == 1 && pp_BumpMap == 0
			color.rgb += offset.rgb;
		#endif
	}
	#endif

	color = colorClamp(color);

	#if pp_FogCtrl == 0
		color.rgb = mix(color.rgb, uniformBuffer.sp_FOG_COL_RAM.rgb, fog_mode2(vtx_uv.z));
	#endif
	#if pp_FogCtrl == 1 && pp_Offset == 1 && pp_BumpMap == 0
		color.rgb = mix(color.rgb, uniformBuffer.sp_FOG_COL_VERT.rgb, offset.a);
	#endif

	#if pp_TriLinear == 1
		color *= pushConstants.trilinearAlpha;
	#endif

	#if cp_AlphaTest == 1
		color.a = round(color.a * 255.0) / 255.0;
		if (uniformBuffer.cp_AlphaTestValue > color.a)
			discard;
		color.a = 1.0;
	#elif GBUFFER == 1 && IS_TRANSLUCENT == 1
		if (color.a < 0.2)
			discard;
	#endif

#if DIV_POS_Z == 1
	highp float w = 100000.0 / vtx_uv.z;
#else
	highp float w = 100000.0 * vtx_uv.z;
#endif
	highp float log_z = log2(1.0 + max(w, -0.999999)) / 34.0;

#if GBUFFER == 1
	gl_FragDepth = log_z;
	// Normale geometrique + interpolee si disponible
	vec3 geoNormal = normalize(cross(dFdx(vtx_pos), dFdy(vtx_pos)));
	vec3 N = (length(vtx_normal) > 0.001) ? normalize(vtx_normal) : geoNormal;

	FragColor = color;
	NormalColor = vec4(N * 0.5 + 0.5, 1.0);

	// Material ID encode sur 8 bits
	// Bit 7 : Presence (Toujours 1 pour la geometrie), Bits 6-4 : list_type, Bit 3 : texture, Bit 2 : gouraud, Bit 1 : bumpmap, Bit 0 : fog/palette
	uint matID = 0u;
	matID |= (1u << 7); // Bit de presence obligatoire

	#if cp_AlphaTest == 1
		matID |= (4u << 4);  // ListType_Punch_Through = 4
	#elif IS_TRANSLUCENT == 1
		matID |= (2u << 4);  // ListType_Translucent = 2
	#endif

	#if pp_Texture == 1
		matID |= (1u << 3);
	#endif
	#if pp_Gouraud == 1
		matID |= (1u << 2);
	#endif
	#if pp_BumpMap == 1
		matID |= (1u << 1);
	#endif
	#if pp_FogCtrl == 0 || pp_FogCtrl == 1
		matID |= 1u;
	#endif
	MaterialColor = matID;

	// Motion buffer : velocite per-poly encodee [0,1]
	MotionColor = pushConstants.velocity * 0.5 + 0.5;

	// HUD Separation
	if (pushConstants.isHUD > 0.5) {
		HUDColor = color;
		FragColor = vec4(0.0, 0.0, 0.0, 0.0);
	} else {
		HUDColor = vec4(0.0);
	}
#else
	#if DITHERING == 1
	{
		float ditherTable[16] = float[](
			5., 13.,  7., 15.,
			9.,  1., 11.,  3.,
			6., 14.,  4., 12.,
			10., 2.,  8.,  0.
		);
		float dr = ditherTable[int(mod(gl_FragCoord.y, 4.)) * 4 + int(mod(gl_FragCoord.x, 4.))];
		vec4 dv = vec4(dr, dr, dr, 1.) / uniformBuffer.ditherDivisor;
		color = clamp(floor(color * 255. + dv) / 255., 0., 1.);
	}
	#endif
	FragColor = color;
#endif
}

