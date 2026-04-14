# Implémentation du SSAO (Screen Space Ambient Occlusion) dans Flycast

Ce document explique la démarche technique utilisée pour implémenter une passe de SSAO correcte dans le moteur de rendu Vulkan de Flycast.

## 1. Architecture Globale
L'implémentation repose sur une architecture de rendu différé (G-Buffer) où la géométrie est d'abord tracée pour extraire ses propriétés physiques, suivie d'une passe de post-process pour calculer l'occlusion.

### Composants du G-Buffer
- **Albedo (Color)** : `eR8G8B8A8Unorm`
- **Normals** : `eR16G16B16A16Sfloat` (pour une précision accrue des vecteurs normaux)
- **Depth Buffer** : Partagé et exposé en tant que texture échantillonnable (`eSampled`).

---

## 2. Modifications du `ScreenDrawer`
Pour permettre au SSAO de lire les données de profondeur et de normales, le `ScreenDrawer` a été modifié :
- **Exposition des Attachements** : Ajout de méthodes `GetDepthAttachment()` et `GetColorAttachment()` pour permettre au renderer d'accéder aux textures du G-Buffer.
- **Configuration Vulkan** : Le buffer de profondeur est désormais créé avec le flag `vk::ImageUsageFlagBits::eSampled` et finit la passe principale dans le layout `eDepthStencilReadOnlyOptimal`.

---

## 3. La Classe `SSAOPass`
Une nouvelle classe `SSAOPass` a été introduite dans `gbuffer_renderer.cpp`. Elle gère son propre cycle de vie Vulkan :

- **Render Pass Dédiée** : Utilise une passe séparée qui écrit directement dans l'attachement Albedo existant.
- **Blending Multiplicatif** : La passe utilise un mode de mélange `eMultiply` (`dstColor = srcColor * dstColor`) pour appliquer l'occlusion ambiante sur l'image déjà calculée.
- **Descriptor Sets** : Lie les textures de profondeur et de normales du G-Buffer comme entrées du shader.

---

## 4. Algorithme du Shader (`vulkan_ssao.frag`)
Le shader calcule l'occlusion en espace écran (UV) :

1. **Reconstruction de Position** : Bien que simplifié en espace UV, il utilise la normale (`normalTex`) et la profondeur (`depthTex`) pour orienter l'échantillonnage.
2. **Hémisphère Orienté** : Un repère tangent (TBN) est construit autour de la normale du fragment pour orienter les échantillons dans l'hémisphère visible.
3. **Échantillonnage Pseudo-Aléatoire** : Utilise 16 échantillons distribués selon le nombre d'or pour une couverture quasi-uniforme, avec une rotation aléatoire par fragment pour réduire les bandes visuelles.
4. **Range Check** : Une fonction `smoothstep` est utilisée pour éviter que des objets très éloignés en profondeur n'occluent le fragment (évite l'effet "halo" noir autour des objets distants).

---

## 5. Intégration dans `GBufferVulkanRenderer`
La passe SSAO est insérée à la fin de la méthode `Render()` :
```cpp
// 1. Passe principale (G-Buffer)
screenDrawer.Draw(...);

// 2. Passe SSAO (Post-process)
if (config::EnableSSAO) {
    ssaoPass.Render(commandBuffer, imageIndex);
}
```

---

## 6. Vérification et Débogage
- **Performance** : L'utilisation d'une passe post-process dédiée évite de recalculer l'AO pour chaque pixel couvert par plusieurs géométries (gain de temps GPU).
- **Précision** : L'utilisation du tampon de normales `R16Sfloat` permet un calcul d'occlusion beaucoup plus stable sur les surfaces courbes.
- **Visualisation** : Les modes de débogage (`Alt+1`, `Alt+2`, `Alt+3`) permettent d'isoler la profondeur, les normales ou l'AO seule pour l'ajustement des paramètres (`SSAO_RADIUS`, `SSAO_STRENGTH`).
