# Projet : Export du Z-Buffer pour Flycast (Vulkan)

Ce document centralise la stratégie technique pour permettre l'utilisation de shaders basés sur la profondeur (DoF, SSGI, Motion Blur) dans l'émulateur Flycast sous Linux/RetroArch.

## I. Roadmap Technique

### Phase 1 : Modification du Cœur Flycast (Upstream)

L'objectif est d'exposer la texture de profondeur sans impacter les performances par défaut.

Localisation : core/rend/vulkan/drawer.cpp, core/rend/vulkan/oit/oit_drawer.cpp et shell/libretro/libretro.cpp.

Action : Ajouter vk::ImageUsageFlagBits::eSampled au FramebufferAttachment du tampon de profondeur.

Toggle : Implémenter une option bool config.ExposeDepth dans les Core Options de Libretro.

### Phase 2 : Validation avec vkBasalt

Utiliser Linux pour intercepter la texture via les outils de post-process système.

Tester avec le shader ReShade DisplayDepth.fx.

Calibrer la linéarisation de la profondeur (conversion du format $1/W$ de la Dreamcast).

### Phase 3 : Extension de l'API Libretro

Proposer une sémantique Depth pour le format de shader .slang.

Créer un pont de communication pour que RetroArch puisse "lier" (bind) l'attachement de profondeur au Sampler2D du shader.

## II. Les Possibilités Offertes par la Z-Map (Potentiel Visuel)

L'accès au tampon de profondeur débloque des effets de rendu de "nouvelle génération" (Post-Process 3D) :

Depth of Field (DoF) : Simulation d'une mise au point optique (bokeh). Permet de flouter l'arrière-plan ou l'avant-plan pour un rendu cinématographique.

SSAO / HBAO+ : (Screen Space Ambient Occlusion) Ajoute des ombres de contact réalistes dans les coins et les zones où les objets se touchent, renforçant la présence physique des modèles.

SSGI (Global Illumination) : Simulation des rebonds de lumière. Les couleurs des objets "débordent" sur les surfaces voisines (Color Bleeding).

SSR (Reflections) : Reflets en temps réel sur les surfaces mouillées ou brillantes (ex: le sol dans Sega GT ou Metropolis Street Racer).

Motion Blur Per-Object : Flou de mouvement basé sur la vitesse réelle des objets à l'écran plutôt qu'un flou d'accumulation global.

Brouillard Volumétrique : Ajout d'une brume dont l'épaisseur varie selon la distance, idéal pour masquer le "pop-in" ou améliorer l'atmosphère des jeux d'aventure.

Correction de Perspective : Possibilité de redresser ou de modifier la géométrie perçue pour des effets d'écran incurvé plus précis.

Edge Smoothing (Anti-Aliasing de profondeur) : Détecter les contours réels des objets 3D pour appliquer un lissage (SMAA/FXAA) plus intelligent sans flouter l'interface utilisateur (HUD).

## III. Guide d'implémentation Vulkan (C++)

Pour exposer la texture, la modification principale se situe lors de l'initialisation des ressources graphiques.

```
// Exemple de modification dans le setup du Depth Buffer (drawer.cpp)

vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
if (config::ExposeDepth)
    usage |= vk::ImageUsageFlagBits::eSampled;
else
    usage |= vk::ImageUsageFlagBits::eTransientAttachment;

depthAttachment->Init(width, height, format, usage, "DEPTH ATTACHMENT");
```

## IV. Scripts de Shading (Slang)

### 1. Shader de Flou Standard (Color-only)

```
#version 450
layout(push_constant) uniform Push { vec4 SourceSize; } params;
layout(set = 0, binding = 0) uniform sampler2D Source;
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;

void main() {
vec2 offset = 1.0 / params.SourceSize.xy;
vec4 color = texture(Source, vTexCoord) * 0.4;
color += texture(Source, vTexCoord + vec2(offset.x, 0.0)) * 0.3;
color += texture(Source, vTexCoord - vec2(offset.x, 0.0)) * 0.3;
FragColor = color;
}
```

### 2. Shader "Depth Aware" (DoF)

```
#version 450
layout(push_constant) uniform Push {
vec4 SourceSize;
float FocusPoint;
float BlurStrength;
} params;

layout(set = 0, binding = 0) uniform sampler2D Source;
layout(set = 0, binding = 1) uniform sampler2D Depth;

layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;

void main() {
float raw_z = texture(Depth, vTexCoord).r;
float dist = abs(raw_z - params.FocusPoint);
float factor = clamp(dist * params.BlurStrength, 0.0, 1.0);

    vec2 offset = (1.0 / params.SourceSize.xy) * factor;
    vec4 color = texture(Source, vTexCoord) * 0.4;
    color += texture(Source, vTexCoord + vec2(offset.x, 0.0)) * 0.3;
    color += texture(Source, vTexCoord - vec2(offset.x, 0.0)) * 0.3;
    FragColor = color;
}
```

## V. Commandes utiles (Linux)

### Test d'injection

```
ENABLE_VKBASALT=1 retroarch -L ./flycast_libretro.so
```
