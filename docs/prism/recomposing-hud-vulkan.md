
Tu es un développeur expert en C++ et Vulkan. Ton objectif est d'intégrer une nouvelle classe `VulkanHudCompositor` dans notre moteur de rendu.

Tu as accès aux fichiers `VulkanHudCompositor.h` et `VulkanHudCompositor.cpp`.

### Contexte Architectural
Nous passons d'un ancien système de HUD à un système de composition par transfert de blocs (BitBlt) optimisé.
Le HUD est d'abord dessiné par notre fragment shader (`main.frag`) dans un attachement de notre GBuffer. La classe `VulkanHudCompositor` prend cette texture en entrée, extrait les zones définies, et les copie vers l'image finale de la swapchain.

### Tâches à accomplir :

1. **Instanciation :**
    - Ajoute une instance de `VulkanHudCompositor` (par exemple `m_hudCompositor`) dans notre classe principale gérant le rendu (ex: `VulkanRenderer` ou `VulkanContext`).

2. **Initialisation et Destruction :**
    - Trouve l'endroit où la Swapchain est créée/initialisée. Ajoute un appel à `m_hudCompositor.Init(renderViewportExtent, swapchainImageCount)`.
    - **Attention :** Utilise bien la résolution de rendu interne (GBuffer), pas la taille de la fenêtre si elles sont différentes.
    - Appelle `m_hudCompositor.Term()` dans la fonction de nettoyage/shutdown du moteur.

3. **Gestion du Redimensionnement (Resize) :**
    - Dans le callback de redimensionnement (lorsque le GBuffer ou la Swapchain est recréé), ajoute un appel à `m_hudCompositor.UpdateViewport(newRenderViewportExtent)`.

4. **Intégration dans la Boucle de Rendu (Command Buffer) :**
    - Localise l'enregistrement de notre Command Buffer, juste après la passe de rendu qui écrit dans le GBuffer.
    - Avant d'appeler le compositeur, tu DOIS ajouter des barrières (pipeline barriers) pour préparer les layouts :
        - La texture source du HUD (attachement du GBuffer) doit passer en `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`.
        - L'image de destination (Swapchain) doit passer en `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL`.
    - Appelle ensuite `m_hudCompositor.Recompose(cmd, currentImageIndex, hudElements, srcGbufferHudImage, dstSwapchainImage)`.
    - Enfin, ajoute une transition pour passer l'image de la Swapchain de `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL` vers `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` (prête pour l'affichage).

### Contraintes strictes :
- Ne modifie pas le code interne de `VulkanHudCompositor`. La classe gère déjà ses propres barrières de synchronisation mémoire et le clipping interne.
- Assure-toi que la liste `hudElements` fournie à la méthode `Recompose` ne contienne que les éléments qui nécessitent d'être copiés à cette frame.

Analyse mon code actuel et propose-moi les modifications à apporter dans mes fichiers principaux pour réaliser cette intégration.