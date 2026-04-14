R"(
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

		#if pp_IgnoreTexA == 1
			texcol.a = 1.0;
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
	}
	#endif

	color = colorClamp(color);

	#if cp_AlphaTest == 1
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
	gl_FragDepth = log_z;

#if GBUFFER == 1
	// Normale geometrique + interpolee si disponible
	vec3 geoNormal = normalize(cross(dFdx(vtx_pos), dFdy(vtx_pos)));
	vec3 N = (length(vtx_normal) > 0.001) ? normalize(vtx_normal) : geoNormal;

	// Toujours écrire l'albedo dans FragColor (attachment 0) pour que le SSAO fonctionne correctement
	FragColor = color;
	if (ShowDepth == 1) {
		// Mode debug profondeur : on encode la profondeur dans NormalColor (attachment 1)
		NormalColor = vec4(vec3(log_z), 1.0);
	} else {
		// Mode normal ou ShowNormals : normales dans NormalColor (attachment 1)
		NormalColor = vec4(N * 0.5 + 0.5, 1.0);
	}
#else
	FragColor = color;
#endif
}
)"