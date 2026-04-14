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

float getLinearDepth(float d)
{
    // On suppose que la profondeur est deja lineaire dans le G-Buffer
    return d;
}

void main()
{
    vec3 centerColor = texture(albedoTex, inUV).rgb;
    float centerDepth = getLinearDepth(texture(depthTex, inUV).r);
    
    float coc = clamp(abs(centerDepth - pc.focus) * pc.intensity * 10.0, 0.0, 1.0);
    
    if (coc < 0.01) {
        FragColor = vec4(centerColor, 1.0);
        return;
    }

    vec3 blurColor = vec3(0.0);
    float totalWeight = 0.0;
    float radius = coc * 10.0; // Rayon de flou max
    
    // Tentative de flou bilateral simplifie en une passe pour commencer (optimisable en 2 passes)
    for (float x = -2.0; x <= 2.0; x += 1.0) {
        for (float y = -2.0; y <= 2.0; y += 1.0) {
            vec2 offset = vec2(x, y) * radius / pc.resolution;
            vec3 c = texture(albedoTex, inUV + offset).rgb;
            float d = getLinearDepth(texture(depthTex, inUV + offset).r);
            
            // Poids bilateral basé sur la difference de profondeur
            float weight = exp(-(x*x + y*y) / 2.0);
            float depthWeight = exp(-abs(d - centerDepth) * 100.0);
            weight *= depthWeight;
            
            blurColor += c * weight;
            totalWeight += weight;
        }
    }
    
    FragColor = vec4(blurColor / max(totalWeight, 0.0001), 1.0);
}
)"
