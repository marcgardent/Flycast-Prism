R"(
layout (std140, set = 1, binding = 2) uniform N2VertexShaderUniforms
{
	mat4 mvMat;
	mat4 normalMat;
	mat4 projMat;
	ivec2 envMapping;
	int bumpMapping;
	int polyNumber;

	vec2 glossCoef;
	ivec2 constantColor;
} n2Uniform;

#define PI 3.1415926

#define LMODE_SINGLE_SIDED 0
#define LMODE_DOUBLE_SIDED 1
#define LMODE_DOUBLE_SIDED_WITH_TOLERANCE 2
#define LMODE_SPECIAL_EFFECT 3
#define LMODE_THIN_SURFACE 4
#define LMODE_BUMP_MAP 5

#define ROUTING_SPEC_TO_OFFSET 1
#define ROUTING_DIFF_TO_OFFSET 2
#define ROUTING_ATTENUATION 1	// not handled
#define ROUTING_FOG 2			// not handled
#define ROUTING_ALPHA 4
#define ROUTING_SUB 8

struct N2Light
{
	vec4 color;
	vec4 direction;	// For parallel/spot
	vec4 position;	// For spot/point

	int parallel;
	int routing;
	int dmode;
	int smode;

	ivec2 diffuse;
	ivec2 specular;

	float attnDistA;
	float attnDistB;
	float attnAngleA;	// For spot
	float attnAngleB;

	int distAttnMode;	// For spot/point
	int _pad1;
	int _pad2;
	int _pad3;
};

layout (std140, set = 1, binding = 3) uniform N2Lights
{
	N2Light lights[16];
	vec4 ambientBase[2];
	vec4 ambientOffset[2];
	ivec2 ambientMaterialBase;
	ivec2 ambientMaterialOffset;
	int lightCount;
	int useBaseOver;
	int bumpId0;
	int bumpId1;
} n2Lights;

void computeColors(inout vec4 baseCol, inout vec4 offsetCol, in int volIdx, in vec3 position, in vec3 normal)
{
	if (n2Uniform.constantColor[volIdx] == 1)
		return;
	vec3 diffuse = vec3(0.0);
	vec3 specular = vec3(0.0);
	float diffuseAlpha = 0.0;
	float specularAlpha = 0.0;
	vec3 reflectDir = reflect(normalize(position), normal);
	const float BASE_FACTOR = 2.0;

	for (int i = 0; i < n2Lights.lightCount; i++)
	{
		vec3 lightDir; // direction to the light
		vec3 lightColor = n2Lights.lights[i].color.rgb;
		if (n2Lights.lights[i].parallel == 1)
		{
			lightDir = normalize(n2Lights.lights[i].direction.xyz);
		}
		else
		{
			lightDir = normalize(n2Lights.lights[i].position.xyz - position);
			if (n2Lights.lights[i].attnDistA != 1.0 || n2Lights.lights[i].attnDistB != 0.0)
			{
				float distance = length(n2Lights.lights[i].position.xyz - position);
				if (n2Lights.lights[i].distAttnMode == 0)
					distance = 1.0 / distance;
				lightColor *= clamp(n2Lights.lights[i].attnDistB * distance + n2Lights.lights[i].attnDistA, 0.0, 1.0);
			}
			if (n2Lights.lights[i].attnAngleA != 1.0 || n2Lights.lights[i].attnAngleB != 0.0)
			{
				vec3 spotDir = n2Lights.lights[i].direction.xyz;
				float cosAngle = 1.0 - max(0.0, dot(lightDir, spotDir));
				lightColor *= clamp(cosAngle * n2Lights.lights[i].attnAngleB + n2Lights.lights[i].attnAngleA, 0.0, 1.0);
			}
		}
		if (n2Lights.lights[i].diffuse[volIdx] == 1)
		{
			float factor = (n2Lights.lights[i].routing & ROUTING_SUB) != 0 ? -BASE_FACTOR : BASE_FACTOR;
			if (n2Lights.lights[i].dmode == LMODE_SINGLE_SIDED)
				factor *= max(dot(normal, lightDir), 0.0);
			else if (n2Lights.lights[i].dmode == LMODE_DOUBLE_SIDED)
				factor *= abs(dot(normal, lightDir));

			if ((n2Lights.lights[i].routing & ROUTING_ALPHA) != 0)
				diffuseAlpha += lightColor.r * factor;
			else
			{
				if ((n2Lights.lights[i].routing & ROUTING_DIFF_TO_OFFSET) == 0)
					diffuse += lightColor * factor * baseCol.rgb;
				else
					specular += lightColor * factor * baseCol.rgb;
			}
		}
		if (n2Lights.lights[i].specular[volIdx] == 1)
		{
			float factor = (n2Lights.lights[i].routing & ROUTING_SUB) != 0 ? -BASE_FACTOR : BASE_FACTOR;
			if (n2Lights.lights[i].smode == LMODE_SINGLE_SIDED)
				factor *= clamp(pow(max(dot(lightDir, reflectDir), 0.0), n2Uniform.glossCoef[volIdx]), 0.0, 1.0);
			else if (n2Lights.lights[i].smode == LMODE_DOUBLE_SIDED)
				factor *= clamp(pow(abs(dot(lightDir, reflectDir)), n2Uniform.glossCoef[volIdx]), 0.0, 1.0);

			if ((n2Lights.lights[i].routing & ROUTING_ALPHA) != 0)
				specularAlpha += lightColor.r * factor;
			else
			{
				if ((n2Lights.lights[i].routing & ROUTING_SPEC_TO_OFFSET) == 0)
					diffuse += lightColor * factor * offsetCol.rgb;
				else
					specular += lightColor * factor * offsetCol.rgb;
			}
		}
	}
	// ambient light
	if (n2Lights.ambientMaterialBase[volIdx] == 1)
		diffuse += n2Lights.ambientBase[volIdx].rgb * baseCol.rgb;
	else
		diffuse += n2Lights.ambientBase[volIdx].rgb;
	if (n2Lights.ambientMaterialOffset[volIdx] == 1)
		specular += n2Lights.ambientOffset[volIdx].rgb * offsetCol.rgb;
	else
		specular += n2Lights.ambientOffset[volIdx].rgb;
	baseCol.rgb = diffuse;
	offsetCol.rgb = specular;

	baseCol.a += diffuseAlpha;
	offsetCol.a += specularAlpha;
	if (n2Lights.useBaseOver == 1)
	{
		vec4 overflow = max(baseCol - vec4(1.0), 0.0);
		offsetCol += overflow;
	}
	baseCol = clamp(baseCol, 0.0, 1.0);
	offsetCol = clamp(offsetCol, 0.0, 1.0);
}

void computeEnvMap(inout vec2 uv, in vec3 position, in vec3 normal)
{
	// Spherical mapping
	//vec3 r = reflect(normalize(position), normal);
	//float m = 2.0 * sqrt(r.x * r.x + r.y * r.y + (r.z + 1.0) * (r.z + 1.0));
	//uv += r.xy / m + 0.5;

	// Cheap env mapping
	uv += normal.xy / 2.0 + 0.5;
	uv = clamp(uv, 0.0, 1.0);
}

void computeBumpMap(inout vec4 color0, in vec4 color1, in vec3 position, in vec3 normal, in mat4 normalMat)
{
	// TODO
	//if (n2Lights.bumpId0 == -1)
		return;
	normal = normalize(normal);
	vec3 tangent = color0.xyz;
	if (tangent.x > 0.5)
		tangent.x -= 1.0;
	if (tangent.y > 0.5)
		tangent.y -= 1.0;
	if (tangent.z > 0.5)
		tangent.z -= 1.0;
	tangent = normalize(tangent);
	vec3 bitangent = color1.xyz;
	if (bitangent.x > 0.5)
		bitangent.x -= 1.0;
	if (bitangent.y > 0.5)
		bitangent.y -= 1.0;
	if (bitangent.z > 0.5)
		bitangent.z -= 1.0;
	bitangent = normalize(bitangent);

	float scaleDegree = color0.w;
	float scaleOffset = color1.w;

	vec3 lightDir; // direction to the light
	if (n2Lights.lights[n2Lights.bumpId0].parallel == 1)
		lightDir = n2Lights.lights[n2Lights.bumpId0].direction.xyz;
	else
		lightDir = n2Lights.lights[n2Lights.bumpId0].position.xyz - position;
	lightDir = normalize(lightDir * mat3(normalMat));

	float n = dot(lightDir, normal);
	float cosQ = dot(lightDir, tangent);
	float sinQ = dot(lightDir, bitangent);

	float sinT = clamp(n, 0.0, 1.0);
	float k1 = 1.0 - scaleDegree;
	float k2 = scaleDegree * sinT;
	float k3 = scaleDegree * sqrt(1.0 - sinT * sinT); // cos T

	float q = acos(cosQ);
	if (sinQ < 0.0)
		q = 2.0 * PI - q;

	color0.r = k2;
	color0.g = k3;
	color0.b = q / PI / 2.0;
	color0.a = k1;
	color0 = clamp(color0, 0.0, 1.0);
}
)"
