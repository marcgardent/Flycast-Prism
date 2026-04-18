precision highp float;
precision highp int;

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 fragColor;

layout (set = 0, binding = 0) uniform sampler2D accumulationTex;
layout (set = 0, binding = 1) uniform sampler2D hudTex;

layout (push_constant) uniform PushConstants {
	int viewMode; // 0: Final, 1: Albedo, 2: Normals, 3: Depth, 4: Material, 5: Motion, 6: SSAO, 7: HUD
} pc;

void main() {
	vec4 accColor = texture(accumulationTex, inUV);
	vec4 hud = texture(hudTex, inUV);

	if (pc.viewMode == 0) { // Final HUD Overlay
		vec3 color = accColor.rgb;
		
		// Simple HDR to LDR Tonemapping (Reinhard)
		// color = color / (color + vec3(1.0));
		
		// Gamma correction is usually done by the swapchain if it's sRGB, but if we need it here:
		// color = pow(color, vec3(1.0 / 2.2));

		// Combine HUD (Alpha pre-multiplié)
		color = color * (1.0 - hud.a) + hud.rgb;
		
		fragColor = vec4(color, 1.0);
	} else if (pc.viewMode == 7) { // HUD
		fragColor = hud;
	} else {
		fragColor = vec4(accColor.rgb, 1.0);
	}
}
