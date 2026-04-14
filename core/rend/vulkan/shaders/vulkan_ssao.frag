R"(
precision highp float;
precision highp int;

// --- REGLAGES SSAO ---
// On utilise maintenant les push constants pour ces parametres
#define SSAO_SAMPLES   64
#define PI             3.1415926

layout (set = 0, binding = 0) uniform sampler2D depthTex;
layout (set = 0, binding = 1) uniform sampler2D normalTex;
layout (std140, set = 0, binding = 2) uniform NoiseBlock
{
	vec4 noise[16];
} noiseBlock;

layout (std140, set = 0, binding = 3) uniform KernelBlock
{
	vec4 samples[64];
} kernel;

layout (push_constant) uniform pushBlock
{
	vec2 resolution;
	float nearPlane;
	float farPlane;
	float bias;
	float radius;
	int showSSAO;
} pc;

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 FragColor;

void main()
{
	float depth = texture(depthTex, inUV).r;
	vec3 N = texture(normalTex, inUV).rgb * 2.0 - 1.0;
	N = normalize(N);

	// Position en espace NDC (Z=depth)
	vec3 pos = vec3(inUV * 2.0 - 1.0, depth);

	// Bruit pour la rotation du kernel (index dans la grille 4x4)
	ivec2 noiseCoord = ivec2(mod(gl_FragCoord.xy, 4.0));
	int noiseIdx = noiseCoord.y * 4 + noiseCoord.x;
	vec3 randomVec = noiseBlock.noise[noiseIdx].xyz;

	// Construction de la matrice TBN
	vec3 tangent = normalize(randomVec - N * dot(randomVec, N));
	vec3 bitangent = cross(N, tangent);
	mat3 TBN = mat3(tangent, bitangent, N);

	float occlusion = 0.0;
	for (int i = 0; i < SSAO_SAMPLES; ++i)
	{
		// Echantillon dans l'espace tangent, puis vers l'espace NDC
		vec3 samplePos = TBN * kernel.samples[i].xyz;
		samplePos = pos + samplePos * pc.radius;

		// Offset en espace UV
		vec2 offsetUV = (samplePos.xy + 1.0) * 0.5;
		offsetUV = clamp(offsetUV, vec2(0.0), vec2(1.0));

		float sampleDepth = texture(depthTex, offsetUV).r;

		// Comparaison de profondeur
		float rangeCheck = smoothstep(0.0, 1.0, pc.radius / abs(depth - sampleDepth + 0.0001));
		if (sampleDepth >= samplePos.z + pc.bias)
			occlusion += rangeCheck;
	}

	float ao = 1.0 - (occlusion / float(SSAO_SAMPLES));
	ao = clamp(ao, 0.0, 1.0);

	if (pc.showSSAO == 1) {
		FragColor = vec4(vec3(ao), 1.0);
	} else {
		FragColor = vec4(1.0, 1.0, 1.0, ao);
	}
}
)"
