# Recommandations pour l'implémentation de la Normal Map (Expert Vulkan/Flycast)

Faisant suite au succès de l'implémentation du Z-Map, l'ajout d'une **Normal Map** (carte des normales) est l'étape suivante pour transformer Flycast en un moteur capable de post-processing moderne (SSAO, Relighting, SSR).

## 1. Architecture Technique recommandée

Pour garantir la cohérence avec le Z-Map et les performances, deux approches sont envisageables :

### Option A : Injection Directe (Mode Debug / Immédiat)
Comme pour le `ShowDepth`, nous injectons une constante `ShowNormals` dans le shader de fragment. C'est la méthode la plus simple pour valider visuellement les données.

### Option B : G-Buffer via MRT (Multi-Render Targets) - Solution Cible
À terme, il est recommandé de passer à une architecture de rendu différé (Deferred) où la Couleur, la Profondeur et les Normales sont écrites simultanément dans des buffers séparés.
- **Attachment 0** : `R8G8B8A8_UNORM` (Couleur)
- **Attachment 1** : `R16G16B16A16_SFLOAT` (Normales dans l'espace vue)
- **Attachment 2** : `D32_SFLOAT` (Profondeur)

## 2. Extraction des Normales dans Flycast

Flycast traite deux types de géométrie :
1. **Géométrie standard (Dreamcast/PVR)** : Les normales ne sont généralement pas fournies par le matériel original de la même manière qu'en OpenGL moderne (le PVR travaille par faces et intensités).
2. **Géométrie Naomi 2** : Le moteur supporte déjà des normales par vertex pour le calcul de l'éclairage (Gouraud/Phong).

### Stratégie de reconstruction :
- **Pour Naomi 2** : Utiliser les normales réelles transformées par la `normalMat` (Model-View matrix).
- **Pour Dreamcast standard** : Puisque les normales ne sont pas toujours explicitement présentes, elles doivent être reconstruites dans le fragment shader en utilisant les dérivées partielles de la position :
  ```glsl
  vec3 normal = normalize(cross(dFdx(vpos.xyz), dFdy(vpos.xyz)));
  ```
  *Note : `vpos` doit être la position dans l'espace vue.*

## 3. Gestion des Transparences (Cohérence Z-Map)

Pour que la Normal Map soit raccord avec le Z-Map actuel, il est IMPÉRATIF d'appliquer la même logique de découpage alpha que nous avons mise au point :

```glsl
#if ShowNormals == 1
    #if IS_TRANSLUCENT == 1
        // Seuil alpha identique au Z-Map (0.2)
        if (color.a < 0.2)
            discard;
    #endif
    
    // Encodage des normales : mapping de [-1, 1] vers [0, 1]
    // Utiliser les normales de l'espace vue pour la compatibilité post-process
    vec3 normalOut = normalize(vNormalViewSpace) * 0.5 + 0.5;
    color.rgb = normalOut;
    color.a = 1.0;
#endif
```

## 4. Points de vigilance (Expert Tips)

1. **Espace de coordonnées** : Toujours travailler dans l'**espace Vue (View Space)**. Les normales doivent être relatives à la caméra pour que les effets comme le SSAO fonctionnent indépendamment de l'orientation du monde.
2. **Normalisation** : Ne jamais supposer que les normales interpolées sont unitaires. Un `normalize()` dans le fragment shader est indispensable après l'interpolation.
3. **Bump Mapping** : Flycast possède une implémentation spécifique du Bump Mapping (basée sur l'alpha et les offsets). Si le mode Bump est actif, la Normal Map doit refléter la normale perturbée et non la normale de la face.
4. **Précision** : Pour une Normal Map de qualité, évitez le format `R8G8B8` qui cause du banding. Privilégiez du `RGB10A2` ou `RGBA16F` si le matériel le permet.

## 5. État Actuel de l'implémentation (Debug Mode)

L'implémentation du `ShowNormals` a été réalisée avec succès en suivant ces principes :
- **Reconstruction via Dérivées** : Utilisation de `normalize(cross(dFdx(vtx_pos), dFdy(vtx_pos)))` dans le fragment shader, garantissant une visualisation des normales même sur la géométrie Dreamcast/PVR standard qui n'en possède pas nativement.
- **Propagation de Position** : La position dans l'espace NDC (`vpos.xyz`) est maintenant propagée via l'interpolant `vtx_pos` (location 3) du vertex shader au fragment shader.
- **Cohérence Alpha** : Le seuil de 0.2 est appliqué aux objets translucides, assurant que la Normal Map est parfaitement raccord avec le Z-Map.
- **Activation par Défaut** : L'option `rend.ShowNormals` est activée par défaut pour faciliter les tests immédiats.

---
*Ce document a été mis à jour suite à l'implémentation réussie de la visualisation des normales.*
