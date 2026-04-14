R"(
precision highp float;

layout (set = 0, binding = 0) uniform sampler2D albedoTex;
layout (set = 0, binding = 1) uniform sampler2D depthTex;

layout (push_constant) uniform pushBlock
{
	vec2 resolution;
	float focus;
	float intensity;
} pc;

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 FragColor;

void main()
{
    vec3 centerColor = texture(albedoTex, inUV).rgb;
    // La profondeur est log_z = log2(1 + w) / 34.0, dans [0, 1]
    // On lit directement depuis la texture depth (composante r)
    float centerDepth = texture(depthTex, inUV).r;

    // CoC : distance entre la profondeur du fragment et le plan de focus
    // pc.focus dans [0,1] (profondeur log), pc.intensity controle l'intensite du flou
    float coc = clamp(abs(centerDepth - pc.focus) * pc.intensity * 5.0, 0.0, 1.0);

    if (coc < 0.005) {
        FragColor = vec4(centerColor, 1.0);
        return;
    }

    vec3 blurColor = vec3(0.0);
    float totalWeight = 0.0;
    // Rayon en pixels : max 8px pour eviter un flou trop agressif
    float radius = coc * 8.0;

    for (float x = -2.0; x <= 2.0; x += 1.0) {
        for (float y = -2.0; y <= 2.0; y += 1.0) {
            vec2 offset = vec2(x, y) * radius / pc.resolution;
            vec2 sampleUV = clamp(inUV + offset, vec2(0.0), vec2(1.0));
            vec3 c = texture(albedoTex, sampleUV).rgb;
            float d = texture(depthTex, sampleUV).r;

            float gaussWeight = exp(-(x*x + y*y) / 2.0);
            // Bilateral : rejeter les echantillons trop eloignes en profondeur (evite le bleeding)
            float depthDiff = abs(d - centerDepth);
            float depthWeight = exp(-depthDiff * 50.0);
            float weight = gaussWeight * depthWeight;

            blurColor += c * weight;
            totalWeight += weight;
        }
    }

    FragColor = vec4(blurColor / max(totalWeight, 0.0001), 1.0);
}
)"
