# Post-Mortem : Visualisation du Z-Map et Réflexions sur une Architecture Deferred dans Flycast

## 1. Ce qui a marché (Successes)
- **Injection Directe via Shader Patching** : L'utilisation de constantes GLSL injectées au moment de la compilation du shader (`ShowDepth`) a permis une visualisation robuste sans modifier la structure complexe des passes de rendu existantes.
- **Utilisation de `gl_FragDepth`** : Pour Flycast (Dreamcast), la profondeur est logarithmique. L'affichage de la valeur finale écrite dans le buffer de profondeur (`gl_FragDepth`) a fourni la représentation la plus fidèle possible du calcul de visibilité du matériel original.
- **Invalidation Automatique du Cache** : L'intégration de l'option `ShowDepth` dans le hash du pipeline Vulkan a garanti que le changement d'option à la volée régénère correctement les shaders, évitant ainsi des états de pipeline incohérents.
- **Filtrage par Liste de Rendu & Alpha Thresholding** : L'ajout de l'option `ShowDepthOpaqueOnly` permet d'exclure ou d'inclure les polygones translucides. La découverte clé a été d'appliquer un **Alpha Test forcé (seuil de 0.2)** sur les objets translucides (poussière, fumée) pour simuler le comportement du Punch-Through (arbres). Cela a produit un Z-Map géométrique extrêmement "propre" et sans compromis visuel.
- **Découpage des Transparences** : En traitant les translucides comme des objets opaques avec `discard` binaire uniquement lors de l'affichage Z-Map, on obtient une carte de profondeur qui capture la structure des effets volumétriques tout en restant lisible.
- **Découplage de la Visualisation** : Désactiver le blending et le dithering lors de l'activation du Z-Map a permis d'obtenir une carte de profondeur "propre", essentielle pour le debugging.

## 2. Ce qui n'a pas marché (Failures & Lessons Learned)
- **Perte de l'Early-Z Culling** : L'écriture dans `gl_FragDepth` (même pour une simple visualisation) force le GPU à désactiver l'Early-Z. Sur des scènes lourdes, cela impacte les performances car le GPU ne peut plus rejeter les pixels cachés avant l'exécution du fragment shader.
- **`gl_FragCoord.z` vs `gl_FragDepth`** : `gl_FragCoord.z` est souvent saturé (proche de 1.0) dans les systèmes à profondeur inversée ou logarithmique, ce qui rend la visualisation "tout blanc" inutile. La profondeur réelle calculée manuellement reste la seule source fiable.
- **Saturation et Dynamique** : Appliquer un multiplicateur arbitraire (ex: `* 10.0`) pour "mieux voir" est risqué car il sature rapidement les blancs, faisant perdre toute nuance dans les zones de moyenne distance.

## 3. Vers une Architecture Deferred-ready (Recommandations Expert)

Pour exporter la Z-Map (et d'autres attributs comme les normales ou l'albedo) de manière performante, Flycast devrait évoluer vers une approche de type **G-Buffer**. Voici les étapes recommandées :

### A. Conception du G-Buffer (Multiple Render Targets - MRT)
Au lieu de rendre directement vers le swapchain image, le pipeline doit rendre vers plusieurs attachements simultanément :
- **Attachment 0 (Color)** : RGBA8 (Albedo + Alpha).
- **Attachment 1 (Depth)** : R32_SFLOAT (Profondeur logarithmique linéaire, exportable sans perte).
- **Attachment 2 (Normals/Misc)** : RGBA16F (Si besoin de post-process avancés comme le SSAO).

### B. Gestion des Sous-passes (Subpasses Vulkan)
Utiliser les `VkSubpass` pour optimiser la consommation de bande passante mémoire (Tile-based Rendering) :
- **Subpass 0 (Geometry)** : Remplit le G-Buffer.
- **Subpass 1 (Lighting/Post-Process)** : Lit le G-Buffer via des `input attachments`. C'est ici que l'export de la Z-Map ou l'application d'effets doit avoir lieu.

### C. Préservation de l'Early-Z
- Pour restaurer les performances, il faut minimiser l'usage de `gl_FragDepth`. Si possible, pré-calculer la profondeur dans une passe de **Depth-Prepass** pure, puis l'utiliser en `ReadOnly` dans la passe de géométrie.

### D. Pipeline de Post-Process Dédié
- Implémenter une passe de "Blit" finale ou un "Fullscreen Quad" qui prend en entrée la texture de profondeur du G-Buffer. Cela permettrait d'exporter la Z-Map vers un fichier ou une interface externe sans impacter le rendu principal de l'émulateur.

---
*Document rédigé par Junie (Expert Vulkan Graphics Engineer)*
