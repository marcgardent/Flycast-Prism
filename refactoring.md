### Analyse de l'Architecture pour un Moteur de Rendu Différé (G-Buffer)

L'implémentation d'une architecture modulaire "Vulkan-G-Buffer" au sein de Flycast est tout à fait réalisable sans régression sur les modes OpenGL et Vulkan actuels. Voici une proposition technique détaillée pour structurer ce développement.

### 1. Structure Modulaire du Code
Le code actuel de Flycast est déjà bien segmenté via l'interface `Renderer` (dans `core/hw/pvr/Renderer_if.h`). Pour ajouter un mode G-Buffer, nous devons suivre la hiérarchie suivante :

- **`BaseVulkanRenderer`** : Gère les ressources communes (Context, Textures, Fog, Palette).
- **`VulkanRenderer`** : L'implémentation Forward actuelle (Simple Color + Depth).
- **`OITVulkanRenderer`** : L'implémentation actuelle pour la transparence (Order Independent Transparency).
- **`GBufferVulkanRenderer` (Nouveau)** : Spécialisé dans le remplissage de plusieurs textures (MRT) et l'application d'une passe d'éclairage finale.

### 2. Plan d'Implémentation du "Vulkan-G-Buffer"

#### A. Extension du Système de Configuration
Pour intégrer proprement le nouveau mode dans l'interface et le sélecteur de moteur :
- **`core/types.h`** : Ajouter `Vulkan_GBuffer` à l'énumération `RenderType`.
- **`core/hw/pvr/Renderer_if.cpp`** : Mettre à jour `rend_create_renderer()` pour instancier la nouvelle classe lors de la sélection du mode.

#### B. Refactorisation de la `ScreenDrawer` (Le Cœur du G-Buffer)
Actuellement, `ScreenDrawer` (dans `core/rend/vulkan/drawer.cpp`) est configurée pour un seul attachement de couleur. La modularité passera par :
- Une **configuration dynamique des attachements** dans `ScreenDrawer::Init`.
- Au lieu de `vk::Format colorFormat = GetContext()->GetSwapChainFormat()`, nous passerons une liste de formats pour le G-Buffer :
    - `Attachment 0 (Albedo)` : RGBA8
    - `Attachment 1 (Normals)` : RGBA16F (pour une précision accrue)
    - `Attachment 2 (Depth)` : D32F
    - `Attachment 3 (Velocity)` : RG16F (pour les futurs Motion Vectors)

#### C. Unification des Shaders (MRT Ready)
Pour éviter de multiplier les fichiers de shaders, nous modifierons `core/rend/vulkan/shaders.cpp` pour injecter des directives de compilation :
```glsl
#ifdef GBUFFER_MODE
    layout (location = 0) out vec4 outAlbedo;
    layout (location = 1) out vec4 outNormal;
    layout (location = 2) out vec2 outVelocity;
#else
    layout (location = 0) out vec4 outColor;
#endif
```
Le mode `ShowNormals` actuel pourra alors simplement devenir une "vue" (un simple `blit` ou une passe de copie) de l'attachement `outNormal` du G-Buffer.

### 3. Gestion des Régressions
Pour garantir que rien ne casse sur les modes existants :
1. **Isolation des RenderPass** : Chaque type de renderer possédera son propre `vk::RenderPass` avec ses propres attachements. Le mode Forward classique ne sera pas impacté par les textures supplémentaires du G-Buffer.
2. **Compatibilité ascendante** : Les fonctionnalités de debug actuelles (`Alt+1`, `Alt+2`) resteront fonctionnelles car elles utiliseront les mêmes briques de calcul de normales que nous avons déjà stabilisées, mais redirigées vers le G-Buffer en mode "Vulkan-G-Buffer".
3. **Fallback Automatique** : Si le matériel ne supporte pas le MRT (rare sur Vulkan mais possible sur très vieux mobiles), le système pourra basculer automatiquement sur le `VulkanRenderer` Forward standard.

### 4. Alternative : L'Architecture par "Passes"
Une alternative plus légère à la création d'un renderer complet serait de modifier le `VulkanRenderer` existant pour qu'il devienne un **Multi-Pass Renderer**.
- **Pass 1 (Geometry)** : Remplissage des textures.
- **Pass 2 (Lighting/Debug)** : Composition finale.

C'est l'approche la plus "moderne" car elle permettrait à terme de n'avoir qu'un seul moteur Vulkan capable de tout faire, activant ou désactivant le G-Buffer selon les options graphiques choisies par l'utilisateur.

### Prochaine Étape Recommandée
Commencer par la **refactorisation de `ScreenDrawer`** pour lui permettre de créer et gérer plusieurs textures d'attachement (MRT), ce qui est le prérequis technique indispensable pour toute forme de rendu différé.