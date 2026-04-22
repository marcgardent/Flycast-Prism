Tu es un développeur expert en C++ et Vulkan. Ton objectif est d'intégrer une nouvelle classe `VulkanHudCompositor` dans notre moteur de rendu. Tu as accès aux fichiers `VulkanHudCompositor.h` et `VulkanHudCompositor.cpp`.

### Contexte Architectural & Pipeline
Notre architecture de HUD fonctionne ainsi :
1. **Source :** Notre fragment shader (`main.frag`) dessine le HUD complet dans une texture dédiée de notre GBuffer (`srcGbufferHud`).
2. **Composition (BitBlt) :** La classe `VulkanHudCompositor` prend cette texture source, extrait les rectangles des fenêtres/éléments demandés, et les copie vers une texture HUD finale (`dstHudCompositionImage`).
3. **Blending Final :** Une passe de rendu ultérieure (Full Screen Quad) prend cette `dstHudCompositionImage` et la mélange (Alpha Blending) par-dessus notre scène 3D finale.

### Tâches d'intégration à accomplir :

1. **Instanciation & Cycle de vie :**
   - Ajoute une instance `m_hudCompositor` dans notre classe de rendu.
   - Initialise-la via `m_hudCompositor.Init(renderViewportExtent, swapchainImageCount)`. Utilise bien la résolution interne du GBuffer (`renderViewportExtent`).
   - Gère sa destruction via `Term()` et le redimensionnement via `UpdateViewport(newRenderExtent)`.

2. **Intégration dans le Command Buffer :**
   - Place l'appel au compositeur **après** la passe du GBuffer, mais **avant** la passe de Blending final.
   - **Transitions requises avant l'appel :** - `srcGbufferHud` -> `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`.
      - `dstHudCompositionImage` -> `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL`.
   - Appelle `m_hudCompositor.Recompose(cmd, currentImageIndex, hudElements, srcGbufferHud, dstHudCompositionImage)`.
   - **Transitions requises après l'appel :**
      - `dstHudCompositionImage` -> `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL` (pour qu'elle puisse être lue par le fragment shader de la passe de blending final).

### 🚨 CONTRAINTES STRICTES ET PIÈGES À ÉVITER 🚨

1. **Le Piège de l'Alpha Blending (Destination cible) :**
   - La méthode `Recompose` effectue une copie mémoire brute (DMA BitBlt), **elle ne gère pas la transparence**.
   - Par conséquent, tu ne dois **JAMAIS** passer l'image finale de la Swapchain comme `dstImage` à `Recompose`. Si tu le fais, les zones transparentes du HUD écraseront la scène 3D avec des pixels noirs. La cible de `Recompose` DOIT être une texture intermédiaire (`dstHudCompositionImage`).

2. **L'initialisation Alpha (Transparence de base) :**
   - La classe `VulkanHudCompositor` s'occupe déjà de nettoyer son buffer interne à chaque frame (`vkCmdFillBuffer` avec des zéros). Cela garantit que les zones où aucun élément HUD n'est copié auront un Alpha = 0. Ne rajoute pas de `vkCmdClearColorImage` superflu sur l'image cible, le BitBlt final écrasera tout de toute façon.

Analyse mon code actuel et propose-moi les modifications pour créer `dstHudCompositionImage` si elle n'existe pas, configurer la passe de blending correctement, et intégrer les appels au compositeur.