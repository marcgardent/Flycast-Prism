precision highp float;
precision highp int;

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 fragColor;

layout (set = 0, binding = 0) uniform sampler2D albedoTex;
layout (set = 0, binding = 1) uniform sampler2D normalTex;
layout (set = 0, binding = 2) uniform sampler2D depthTex;
layout (set = 0, binding = 3) uniform usampler2D materialTex;
layout (set = 0, binding = 4) uniform sampler2D motionTex;
layout (set = 0, binding = 5) uniform sampler2D ssaoTex;
layout (set = 0, binding = 6) uniform sampler2D hudTex;

layout (push_constant) uniform PushConstants {
	int viewMode; // 0: Final, 1: Albedo, 2: Normals, 3: Depth, 4: Material, 5: Motion, 6: SSAO, 7: HUD
} pc;

vec3 hashColor(uint id) {
	if (id == 0u) return vec3(0.1, 0.1, 0.1);
	uint h = id * 2654435761u;
	return vec3(float((h >> 16u) & 255u) / 255.0,
	            float((h >> 8u)  & 255u) / 255.0,
	            float( h         & 255u) / 255.0);
}

void main() {
	vec4 albedo = texture(albedoTex, inUV);
	vec3 normal = texture(normalTex, inUV).xyz;
	float depth = texture(depthTex, inUV).r;
	uint matID = texture(materialTex, inUV).r;
	vec2 motion = texture(motionTex, inUV).xy;
	float ao = texture(ssaoTex, inUV).r;
	vec4 hud = texture(hudTex, inUV);

	if (pc.viewMode == 1) { // Albedo
		fragColor = vec4(albedo.rgb, 1.0);
	} else if (pc.viewMode == 2) { // Normals
		fragColor = vec4(normal, 1.0);
	} else if (pc.viewMode == 3) { // Depth
		fragColor = vec4(vec3(depth), 1.0);
	} else if (pc.viewMode == 4) { // Material
		fragColor = vec4(hashColor(matID), 1.0);
	} else if (pc.viewMode == 5) { // Motion
		fragColor = vec4(motion, 0.5, 1.0);
	} else if (pc.viewMode == 6) { // SSAO
		fragColor = vec4(vec3(ao), 1.0);
	} else if (pc.viewMode == 7) { // HUD
		fragColor = hud;
	} else { // 3D Resolve
		vec3 color = albedo.rgb;
		
		// Apply SSAO directly here (non-destructive pipeline)
		color *= ao;

		// Combine HUD (note: might be better to move this entirely to HUD Overlay in the future, but preserving logic for now)
		color = mix(color, hud.rgb, hud.a);
		fragColor = vec4(color, 1.0);
	}
}
