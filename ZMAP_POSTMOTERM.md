### Post-Mortem : Tentative d'implémentation de la visualisation Z-Map via PresentFrame

**Auteur :** Junie (AI)  
**Date :** 12 Avril 2026  
**Objectif initial :** Afficher le buffer de profondeur (Z-Map) de Flycast mélangé à 50% avec l'image couleur pour valider la qualité du rendu.

---

### 1. Ce qui a été tenté
*   **Interception de `PresentFrame`** : Détournement de la boucle de présentation Vulkan pour envoyer non pas une, mais deux textures (`Color` + `Depth`) au `VulkanContext`.
*   **Blending manuel** : Ajout d'un second passage de rendu dans `DrawFrame` utilisant un pipeline de quad avec mélange alpha (`SrcAlpha, OneMinusSrcAlpha`).
*   **Transitions de Layout** : Ajout de barrières mémoire explicites pour transformer le buffer de profondeur (normalement en `DepthStencilAttachmentOptimal`) en `ShaderReadOnlyOptimal` pour l'échantillonnage.
*   **Expansion du Descriptor Pool** : Augmentation de la taille du pool de descripteurs (jusqu'à 2 millions) pour absorber la charge supplémentaire.

---

### 2. Ce qui n'a PAS marché
*   **Mélange via DescriptorSets externes** : L'allocation de nouveaux `DescriptorSets` à chaque frame pour gérer la texture de profondeur a causé une fuite mémoire massive (`ErrorOutOfPoolMemory`).
*   **Réinitialisation périodique du Pool** : La tentative de `resetDescriptorPool` toutes les 1000 frames a provoqué des crashs `SIGSEGV` et des erreurs de validation, car le pool contient des descripteurs encore utilisés par d'autres parties du moteur.
*   **Synchronisation CPU/GPU** : La suppression du cache de descripteurs pour stopper la fuite a fait chuter les performances de **30 FPS à 1 FPS**, rendant l'émulateur inutilisable.
*   **Écran Noir Persistant** : Malgré les barrières de layout, le buffer de profondeur échantillonné via `PresentFrame` restait noir, suggérant que les données étaient perdues à la fin de la render pass originale ou que l'aspect `eDepth` n'était pas correctement interprété par le shader de quad générique.

---

### 3. Ce qui a marché (Points de succès)
*   **Isolation des Logs** : Le filtrage exclusif sur la catégorie `RENDERER` a permis de voir enfin les erreurs de validation Vulkan réelles sans le bruit du SH4/Holly.
*   **Détection des Handles Invalides** : L'ajout de vérifications binaires sur les handles Vulkan (`(VkHandle)imageView != 0`) a permis d'éviter les crashs immédiats lors des transitions 2D/3D.
*   **Augmentation de la Stabilité** : L'agrandissement du pool de descripteurs a permis de passer d'un crash après 10 secondes à une session de plus de 2 minutes.

---

### 4. Recommandations pour le prochain développeur
Si vous devez implémenter la visualisation du Z-Map, **ne passez pas par la boucle de présentation (`PresentFrame`)**. C'est trop complexe à synchroniser avec les ressources internes du moteur.

**Pistes recommandées :**
1.  **Injection dans le Shader de Fragment principal** : Modifier les shaders de Flycast pour qu'ils écrivent directement la valeur de `gl_FragCoord.z` dans le canal couleur si l'option `ShowDepth` est active. C'est la méthode la plus rapide et la plus stable.
2.  **Pass de Rendu dédiée** : Créer une pass de rendu spécifique à la fin de la génération de frame dans `ScreenDrawer` qui effectue une copie (Blit) ou un rendu de la profondeur vers la couleur, avant même d'arriver au `PresentFrame`.
3.  **Vérification des drapeaux d'usage** : S'assurer que le buffer de profondeur est créé avec `vk::ImageUsageFlagBits::eSampled` dès son initialisation dans `drawer.cpp`.

---

### 5. État final du projet
Le projet a été restauré dans un état stable et fluide (30+ FPS). L'option `ShowDepth` est présente mais désactivée par défaut pour éviter les instabilités. Les fondations pour un meilleur système de log et de gestion de mémoire sont prêtes.
