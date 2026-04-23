precision highp float;

// Entrées standard pour un post-process G-Buffer
layout (set = 0, binding = 0) uniform sampler2D albedoTex; // Source HDR (idéalement RGBA16F)
layout (set = 0, binding = 1) uniform sampler2D depthTex;  // Depth Buffer (log_z)

layout (push_constant) uniform pushBlock {
    vec2 resolution;    // Taille de la target (ex: 1920, 1080)
    float focus;        // Distance de focus [0.0 - 1.0] (log_z)
    float intensity;    // Intensité / Ouverture du diaphragme
} pc;

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 FragColor;

// --- CONSTANTES INTERNES (MODE WARRIOR) ---
const float GOLDEN_ANGLE = 2.39996323; // Angle d'or pour la spirale de Fermat
const int MAX_SAMPLES = 64;            // Précision chirurgicale
const float MAX_RADIUS = 24.0;         // Limite interne du flou en pixels

/**
 * Calcul de la luminance pour la pondération énergétique (Rec. 709)
 */
float getLuminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    // 1. Lecture du pixel central et de sa profondeur
    vec4 centerFetch = texture(albedoTex, inUV);
    vec3 centerCol = centerFetch.rgb;
    float centerDepth = texture(depthTex, inUV).r;

    // 2. Calcul du Cercle de Confusion (CoC)
    // Différence absolue entre la profondeur du pixel et le plan de focus
    float coc = abs(centerDepth - pc.focus) * pc.intensity;

    // Rayon effectif en pixels, bridé par la constante interne MAX_RADIUS
    // Le multiplicateur 50.0 permet de transformer le CoC [0,1] en rayon de flou utile
    float effectiveRadius = min(coc * 50.0, MAX_RADIUS);

    // Optimisation : Si le rayon est négligeable (zone de netteté), on sort tôt
    if (effectiveRadius < 0.5) {
        FragColor = vec4(centerCol, 1.0);
        return;
    }

    // 3. Accumulation d'énergie (Gathering Loop)
    vec3 accColor = vec3(0.0);
    float accWeight = 0.0;

    for (int i = 0; i < MAX_SAMPLES; i++) {
        // Distribution via spirale de Fermat pour un disque de flou parfait
        float r = sqrt(float(i) / float(MAX_SAMPLES));
        float theta = float(i) * GOLDEN_ANGLE;

        vec2 offset = vec2(cos(theta), sin(theta)) * r * effectiveRadius;
        vec2 sampleUV = inUV + (offset / pc.resolution);

        // Sampling avec clamp automatique via le sampler (Edge)
        vec3 col = texture(albedoTex, sampleUV).rgb;
        float depth = texture(depthTex, sampleUV).r;

        // --- LOGIQUE DE POIDS BOKEH (HIGHLIGHT BOOST) ---
        // On amplifie l'importance des pixels brillants (HDR)
        float luma = getLuminance(col);

        // Poids non-linéaire : Luminance^2.5 + offset de base
        // C'est ce qui crée les disques de lumière nets et brillants
        float weight = pow(max(luma, 0.0), 2.5) + 0.1;

        // --- PROTECTION CONTRE LE "BLEEDING" (PROFONDEUR) ---
        // On évite que l'arrière-plan flou ne déborde de façon sale sur le premier plan net
        if (depth < centerDepth - 0.005) {
            // L'échantillon est devant le centre (Foreground)
            // On atténue le poids proportionnellement à l'écart de profondeur
            weight *= smoothstep(0.0, 0.005, centerDepth - depth);
        }

        accColor += col * weight;
        accWeight += weight;
    }

    // 4. Normalisation de l'énergie et sortie finale
    vec3 finalColor = accColor / max(accWeight, 0.0001);

    // Léger boost de contraste pour un look plus "Kodak" sur la piste
    finalColor = mix(finalColor, pow(max(finalColor, 0.0), vec3(1.1)), 0.3);

    FragColor = vec4(finalColor, 1.0);
}