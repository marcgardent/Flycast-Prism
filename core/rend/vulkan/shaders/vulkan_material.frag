R"(
precision highp float;
precision highp int;

layout (location = 0) in vec2 TexCoord;
layout (location = 0) out vec4 fragColor;

layout (set = 0, binding = 0) uniform usampler2D u_MaterialBuffer;

vec3 hashColor(uint id) {
	uint h = id * 2654435761u;
	return vec3(float((h >> 16u) & 255u) / 255.0,
	            float((h >> 8u)  & 255u) / 255.0,
	            float( h         & 255u) / 255.0);
}

void main() {
	uint matID = texture(u_MaterialBuffer, TexCoord).r;
	if (matID == 0u) {
		fragColor = vec4(0.1, 0.1, 0.1, 1.0);
	} else {
		fragColor = vec4(hashColor(matID), 1.0);
	}
}
)"
