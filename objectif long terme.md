### Analyse et Recommandation pour l'Architecture G-Buffer de Flycast

L'objectif de transformer Flycast vers une architecture de rendu différé (Deferred Rendering) nécessite une base solide pour la gestion des données géométriques. Suite à l'analyse du code actuel et des récents succès sur la visualisation des normales et de la profondeur, voici la stratégie recommandée.

#### 1. Conserver ou Repartir de Master ?
**Il est fortement recommandé de conserver les modifications actuelles.**
Repartir de `master` serait contre-productif car les briques fondamentales d'un G-Buffer ont déjà été posées et stabilisées dans la branche actuelle :
- **Extraction des Normales** : Le code dans `ta_vtx.cpp` et les shaders permet déjà de récupérer les normales (Naomi 2 ou via dérivées partielles), ce qui est la donnée la plus complexe à obtenir dans un émulateur.
- **Gestion du Z-Buffer** : La visualisation de la profondeur est déjà intégrée et filtrée (translucides).
- **Raccourcis de Debug** : Les outils `Alt+1/2` sont essentiels pour valider que le futur G-Buffer contient des données correctes avant d'implémenter l'éclairage différé.

#### 2. Plan de Refactorisation pour un G-Buffer complet
Pour passer d'un simple "mode debug" à un véritable moteur G-Buffer (MRT - Multi-Render Target), le plan de refactorisation suivant est proposé :

**A. Abstraction de la RenderPass (MRT Ready)**
Actuellement, `ScreenDrawer::Init` configure une `RenderPass` avec seulement 2 attachements (Color + Depth). La refactorisation doit :
- Étendre `ScreenDrawer` pour supporter un pool d'attachements (G-Buffer) :
    - `Attachment 0` : Albedo / Color (R8G8B8A8)
    - `Attachment 1` : Normals (R16G16B16A16_SFLOAT ou R10G10B10A2)
    - `Attachment 2` : Depth (D24_UNORM_S8_UINT ou D32_SFLOAT)
    - `Attachment 3` : Velocity / Motion Vectors (R16G16_SFLOAT) - *Préparation pour le futur*

**B. Évolution des Shaders**
Les shaders ne doivent plus choisir entre afficher la couleur OU la normale via un `#if`. Ils doivent écrire dans tous les attachements simultanément :
```glsl
// Futur Fragment Shader G-Buffer
layout (location = 0) out vec4 outColor;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec2 outVelocity;

void main() {
    outColor = calculateAlbedo();
    outNormal = vec4(normalize(vtx_normal) * 0.5 + 0.5, 1.0);
    outVelocity = calculateMotion();
}
```

**C. Découplage du Rendu Géométrique et du Lighting**
- **Pass 1 (Geometry Pass)** : Rendu de la scène Dreamcast dans le G-Buffer.
- **Pass 2 (Lighting/Composite Pass)** : Un simple quad plein écran qui lit les textures du G-Buffer pour appliquer les effets (Shadows, SSAO, Bloom, Motion Blur).

#### 3. Prochaines Étapes
1. **Modifier `ScreenDrawer`** pour allouer les textures supplémentaires du G-Buffer.
2. **Mettre à jour la `VkRenderPass`** pour inclure les nouveaux attachements de couleur.
3. **Unifier les Shaders** pour qu'ils produisent systématiquement les données de normales et de profondeur dans le G-Buffer, au lieu de simples modes d'affichage exclusifs.

Cette approche transforme Flycast en un moteur moderne tout en restant compatible avec les spécificités du PowerVR.