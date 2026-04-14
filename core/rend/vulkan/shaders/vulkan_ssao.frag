R"(
precision highp float;
precision highp int;

// --- REGLAGES SSAO ---
#define SSAO_SAMPLES   16
#define SSAO_RADIUS    0.05      // Rayon en espace NDC
#define SSAO_STRENGTH  2.5
#define SSAO_BIAS      0.001
#define SSAO_CONTRAST  1.5
#define PI             3.1415926

layout (set = 0, binding = 0) uniform sampler2D depthTex;
layout (set = 0, binding = 1) uniform sampler2D normalTex;

layout (push_constant) uniform pushBlock
{
	vec2 resolution;
	float nearPlane;
	float farPlane;
	int showSSAO;
} pc;

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 FragColor;

// Reconstruit la position en espace view depuis la profondeur
vec3 reconstructPosition(vec2 uv, float depth)
{
	// depth est en espace log, on le remet en NDC [-1,1]
	vec2 ndc = uv * 2.0 - 1.0;
	return vec3(ndc, depth);
}

void main()
{
	float depth = texture(depthTex, inUV).r;
	vec3 N = texture(normalTex, inUV).rgb * 2.0 - 1.0;
	N = normalize(N);

	vec3 pos = reconstructPosition(inUV, depth);

	float occlusion = 0.0;

	// Bruit pseudo-aleatoire par fragment
	float r1 = fract(sin(dot(gl_FragCoord.xy, vec2(127.1, 311.7))) * 43758.5453);
	float r2 = fract(sin(dot(gl_FragCoord.xy, vec2(269.5, 183.3))) * 43758.5453);

	// Construction d'un repere tangent autour de N
	vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	vec3 T = normalize(cross(up, N));
	vec3 B = cross(N, T);

	for (int i = 0; i < SSAO_SAMPLES; ++i)
	{
		float fi = float(i);
		float phi = 2.0 * PI * (fi * 0.618033988 + r1);
		float cosTheta = 1.0 - (fi + 0.5) / float(SSAO_SAMPLES);
		float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

		vec3 sampleDir = T * (cos(phi) * sinTheta)
		               + B * (sin(phi) * sinTheta)
		               + N * cosTheta;

		// Offset en espace UV
		vec2 sampleUV = inUV + sampleDir.xy * SSAO_RADIUS;
		sampleUV = clamp(sampleUV, vec2(0.0), vec2(1.0));

		float sampleDepth = texture(depthTex, sampleUV).r;

		// Comparaison de profondeur : si le sample est plus proche (devant), il occlut
		float rangeCheck = smoothstep(0.0, 1.0, SSAO_RADIUS / abs(depth - sampleDepth + 0.0001));
		if (sampleDepth < depth - SSAO_BIAS)
			occlusion += rangeCheck;
	}

	float ao = 1.0 - (occlusion / float(SSAO_SAMPLES)) * SSAO_STRENGTH;
	ao = clamp(ao, 0.0, 1.0);
	ao = pow(ao, SSAO_CONTRAST);

	if (pc.showSSAO == 1) {
		FragColor = vec4(vec3(ao), 1.0);
	} else {
		FragColor = vec4(1.0, 1.0, 1.0, ao);
	}
}
)"
