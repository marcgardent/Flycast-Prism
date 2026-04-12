### Étude de Faisabilité : Export du Z-Buffer (Vulkan) pour Flycast

#### 1. Analyse Technique du Code Source
Le projet utilise l'API C++ `vulkan.hpp`. Contrairement à ce qui était initialement prévu, les modifications ne se feront pas sur des structures C `VkImageCreateInfo` brutes, mais via l'abstraction `FramebufferAttachment`.

**Points d'intervention identifiés :**
- **Abstraction de Texture :** La classe `FramebufferAttachment` dans `core/rend/vulkan/texture.cpp` gère la création des images. Sa méthode `Init` doit être capable de recevoir des flags d'usage supplémentaires.
- **Rendu Classique :** Dans `core/rend/vulkan/drawer.cpp`, le `depthAttachment` est initialisé avec `vk::ImageUsageFlagBits::eTransientAttachment`. Pour permettre l'échantillonnage externe, ce flag doit être supprimé et `vk::ImageUsageFlagBits::eSampled` doit être ajouté.
- **Rendu OIT (Order Independent Transparency) :** Dans `core/rend/vulkan/oit/oit_drawer.cpp`, une logique similaire doit être appliquée pour les buffers de profondeur utilisés par l'algorithme OIT.

#### 2. Modifications Requises

**A. Libretro Core Options (`shell/libretro/libretro.cpp`) :**
Ajouter une option `flycast_expose_depth` pour activer/désactiver la fonctionnalité.
```cpp
// Dans la liste des options (retro_variable)
{ "flycast_expose_depth", "Expose Depth Texture; disabled|enabled" },

// Dans update_variables()
if (environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
    config::ExposeDepth = !strcmp(var.value, "enabled");
```

**B. Ajustement de l'Usage des Images (`core/rend/vulkan/drawer.cpp`) :**
```cpp
vk::ImageUsageFlags depthUsage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
if (config::ExposeDepth) {
    depthUsage |= vk::ImageUsageFlagBits::eSampled;
} else {
    depthUsage |= vk::ImageUsageFlagBits::eTransientAttachment;
}
depthAttachment->Init(widthPow2, heightPow2, GetContext()->GetDepthFormat(), depthUsage, "RTT DEPTH ATTACHMENT");
```

#### 3. Risques et Limitations
- **Performance :** L'ajout de `eSampled` et la suppression de `eTransientAttachment` empêchent l'utilisation de la mémoire "lazily allocated" sur certains hardwares (notamment les GPU mobiles/Tilers), ce qui augmentera la consommation de bande passante mémoire.
- **Linéarisation :** Le tampon de profondeur de la Dreamcast est souvent en $1/W$. Les shaders (comme ReShade/vkBasalt) devront appliquer une transformation pour obtenir une distance linéaire.
- **OIT :** Le mode de rendu "Per-Pixel" (OIT) utilise déjà des `InputAttachments`. Il faudra s'assurer que l'exposition de la profondeur ne casse pas la logique de lecture/écriture de l'OIT.

#### 4. Conclusion
L'implémentation est **tout à fait faisable** avec un impact minimal sur la structure du code actuel. L'utilisation des abstractions existantes (`FramebufferAttachment`) simplifie grandement l'injection des flags nécessaires.
