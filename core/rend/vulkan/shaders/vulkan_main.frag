R"(
precision highp float;
precision highp int;

// --- REGLAGES SSAO ---
#define SSAO_SAMPLES 16
#define SSAO_RADIUS 0.3        // Rayon en espace world (unités de vtx_pos)
#define SSAO_STRENGTH 2.5      // Intensité de l'occlusion
#define SSAO_BIAS 0.02         // Biais pour eviter l'auto-occlusion
#define SSAO_CONTRAST 1.5      // Contraste final

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

	float ao = 1.0;
	if (EnableSSAO == 1) {
		float occlusion = 0.0;
		vec3 pos = vtx_pos;

		// Bruit pseudo-aleatoire par fragment
		float r1 = fract(sin(dot(gl_FragCoord.xy, vec2(127.1, 311.7))) * 43758.5453);
		float r2 = fract(sin(dot(gl_FragCoord.xy, vec2(269.5, 183.3))) * 43758.5453);

		// Construction d'un repere tangent autour de N
		vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
		vec3 T = normalize(cross(up, N));
		vec3 B = cross(N, T);

		for (int i = 0; i < SSAO_SAMPLES; ++i) {
			// Distribution hemispherique uniforme
			float fi = float(i);
			float phi = 2.0 * PI * (fi * 0.618033988 + r1); // Golden ratio pour quasi-uniforme
			float cosTheta = 1.0 - (fi + 0.5) / float(SSAO_SAMPLES); // Plus de samples pres de la surface
			float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

			// Direction dans l'hemisphere
			vec3 sampleDir = T * (cos(phi) * sinTheta)
			               + B * (sin(phi) * sinTheta)
			               + N * cosTheta;

			// Point test dans l'espace world
			vec3 testPos = pos + sampleDir * SSAO_RADIUS;

			// On utilise la derivee de la position pour estimer si le point
			// se trouve "en dessous" de la surface voisine
			vec3 posNeighbor = pos + dFdx(pos) * (sampleDir.x * SSAO_RADIUS)
			                       + dFdy(pos) * (sampleDir.y * SSAO_RADIUS);

			float expectedDist = dot(testPos - pos, N);
			float actualDist   = dot(posNeighbor - pos, N);

			float diff = actualDist - expectedDist;

			// Si le voisin est plus proche (devant le plan tangent), il occlut
			if (diff > SSAO_BIAS) {
				occlusion += smoothstep(SSAO_RADIUS, 0.0, diff);
			}
		}

		ao = 1.0 - (occlusion / float(SSAO_SAMPLES)) * SSAO_STRENGTH;
		ao = clamp(ao, 0.0, 1.0);
		ao = pow(ao, SSAO_CONTRAST);
	}

	if (ShowSSAO == 1) {
		FragColor = vec4(vec3(ao), 1.0);
		NormalColor = vec4(0.0, 0.0, 0.0, 1.0);
	} else if (ShowDepth == 1) {
		FragColor = vec4(vec3(log_z), 1.0);
		NormalColor = vec4(vec3(log_z), 1.0);
	} else if (ShowNormals == 1) {
		FragColor = vec4(N * 0.5 + 0.5, 1.0);
		NormalColor = vec4(N * 0.5 + 0.5, 1.0);
	} else {
		FragColor = color;
		FragColor.rgb *= ao;
		NormalColor = vec4(N * 0.5 + 0.5, 1.0);
	}
#else
	FragColor = color;
#endif
}
)"