# Guide de Débogage Visuel de la Z-Map (Profondeur)

Ce document propose une configuration temporaire pour afficher directement le tampon de profondeur (Z-Map) à l'écran. Cela permet de vérifier visuellement la précision, la linéarisation et l'absence d'artefacts avant d'appliquer des effets complexes comme le DoF ou le SSAO.

## 1. Shader de Visualisation Directe (Slang)

Créez un fichier nommé `debug_zmap.slang` dans votre dossier de shaders RetroArch. Ce shader affiche la profondeur brute ou linéarisée en niveaux de gris.

```glsl
#version 450

layout(push_constant) uniform Push {
    vec4 SourceSize;
    float ZNear; // Valeur typique: 0.1
    float ZFar;  // Valeur typique: 1000.0
    uint Linearize; // 0 = Brut, 1 = Linéaire
} params;

layout(set = 0, binding = 0) uniform sampler2D Source;
layout(set = 0, binding = 1) uniform sampler2D Depth;

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;

// Fonction de linéarisation pour une projection perspective standard
float linearizeDepth(float z) {
    return (2.0 * params.ZNear) / (params.ZFar + params.ZNear - z * (params.ZFar - params.ZNear));
}

void main() {
    float z = texture(Depth, vTexCoord).r;
    
    if (params.Linearize == 1) {
        z = linearizeDepth(z);
    }

    // Affichage en niveaux de gris
    // Plus l'objet est proche, plus il est blanc (ou inversement selon la logique du jeu)
    FragColor = vec4(vec3(z), 1.0);
}
```

## 2. Modifications C++ pour le Mode Debug

Pour faciliter le débogage, on peut forcer l'affichage du buffer de profondeur directement dans le `drawer.cpp`.

### A. Forcer l'échantillonnage de la profondeur
Dans `core/rend/vulkan/drawer.cpp`, assurez-vous que `eSampled` est activé pour le `depthAttachment` :

```cpp
// Localisation : core/rend/vulkan/drawer.cpp (vers ligne 656)
depthAttachment->Init(viewport.width, viewport.height, GetContext()->GetDepthFormat(),
        vk::ImageUsageFlagBits::eDepthStencilAttachment | 
        vk::ImageUsageFlagBits::eSampled | // Ajout crucial pour le debug
        vk::ImageUsageFlagBits::eTransientAttachment,
        "DEBUG DEPTH ATTACHMENT");
```

### B. Ajout d'une option de "Vue Debug" (Optionnel)
Dans `shell/libretro/libretro.cpp`, vous pouvez ajouter une option pour basculer l'affichage :

```cpp
// Dans la gestion des options Libretro
if (config::DebugZMap) {
    // Code pour rediriger le sampler Depth vers la sortie principale
}
```

## 3. Protocole de Vérification de Qualité

Une fois le shader actif, vérifiez les points suivants :

1.  **Gradient de Gris** : Le passage du noir au blanc doit être fluide. Des "marches d'escalier" indiquent un manque de précision (bit-depth insuffisant).
2.  **Contours** : Les objets doivent avoir des bords nets dans la Z-Map. Un flou sur les bords indique un problème d'alignement des coordonnées UV.
3.  **Z-Fighting** : Si des surfaces clignotent en mode debug, cela confirme un problème de précision qui causera des artefacts dans les shaders de post-process.
4.  **Linéarisation** : En mode "Linéaire", le dégradé doit paraître naturel à l'œil. Si tout est blanc sauf les objets très proches, la formule de linéarisation doit être ajustée.

## 4. Commande de Test Rapide

Lancez RetroArch avec le shader de debug :
```bash
retroarch -L ./flycast_libretro.so --config-append "video_shader=./shaders/debug_zmap.slangp"
```
