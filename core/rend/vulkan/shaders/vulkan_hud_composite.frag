precision highp float;
layout (location = 0) in vec2 TexCoord;
layout (location = 0) out vec4 fragColor;
layout (set = 0, binding = 0) uniform sampler2D u_HUDBuffer;
layout (push_constant) uniform PushConstants {
        float showHUD;
} pc;
void main() {
        vec4 hud = texture(u_HUDBuffer, TexCoord);

        // DEBUG: carre rouge en haut a gauche (10%-30% de l'ecran) pour valider le compositing
        bool inDebugSquare = (TexCoord.x > 0.10 && TexCoord.x < 0.30 && TexCoord.y > 0.10 && TexCoord.y < 0.30);
        if (inDebugSquare) {
                hud = vec4(1.0, 0.0, 0.0, 0.8); // rouge semi-transparent
        }

        if (pc.showHUD > 0.5) {
                fragColor = hud;
        } else {
                fragColor = vec4(0.0, 0.0, 0.0, 0.0);
        }
}

