# Stratégie Motion Vectors pour DLSS, FSR3 et Frame Generation dans Flycast

## Introduction
L'implémentation des vecteurs de mouvement (Motion Vectors - MV) est le chaînon manquant pour débloquer les technologies de pointe comme le **DLSS 3**, le **FSR 3** et surtout la **Frame Generation**. Contrairement aux filtres de post-process classiques, ces technologies nécessitent une connaissance précise du déplacement temporel de chaque fragment de l'image.

## Le Défi : La "Cuisson" CPU (SH-4)
Sur Dreamcast, la géométrie est transformée par le processeur principal (SH-4) avant d'être envoyée au GPU (PVR). Le backend Vulkan de Flycast ne reçoit que des coordonnées "cuites" à l'instant T, sans historique. 

Pour résoudre ce problème et permettre une Frame Generation fluide, nous recommandons la stratégie suivante :

---

## 1. Extension de l'Architecture des Sommets (Vertex)
Il est impératif de transporter la position de la frame précédente (`T-1`) jusqu'au shader de fragment.

### Modification de `core/hw/pvr/ta_ctx.h` :
```cpp
struct Vertex {
    float x, y, z;      // Position actuelle (T)
    float prev_x, prev_y, prev_z; // Position précédente (T-1) <-- NOUVEAU
    // ... reste de la structure (UV, Col, Normales)
};
```

### Injection dans le TA (Tile Accelerator) :
Le moteur de rendu doit maintenir un "Vertex Cache History" indexé par le pointeur de données du jeu. Lorsque `ta_add_vertex` est appelé :
1. Chercher si ce sommet existait à la frame précédente.
2. Si oui, injecter ses coordonnées `(x, y, z)` de T-1 dans `(prev_x, prev_y, prev_z)`.
3. Si non (nouvel objet), mettre `prev_pos = curr_pos` (mouvement nul).

---

## 2. Génération du Velocity Buffer (G-Buffer)
Plutôt que d'injecter des calculs complexes dans le shader de couleur, il faut passer à un rendu **Deferred-Ready** utilisant un MRT (Multiple Render Targets).

- **Attachment 0** : Couleur (RGBA8)
- **Attachment 1** : Profondeur (D32_SFLOAT)
- **Attachment 2** : Normales (RGBA16_SFLOAT)
- **Attachment 3** : **Velocity Buffer** (R16G16_SFLOAT)

Le Velocity Buffer stocke le delta NDC : `velocity = (curr_ndc.xy - prev_ndc.xy)`.

---

## 3. Compatibilité DLSS / FSR 3 / Frame Generation

### Jittering (Antialiasing Temporel)
Pour que le DLSS/FSR fonctionne, le moteur doit appliquer un léger décalage (sub-pixel jitter) aux projections de chaque frame. Ce jitter doit être soustrait du calcul des Motion Vectors pour que ces derniers ne représentent que le **mouvement réel des objets** et non le tremblement de la caméra.

### Frame Generation (Interpolation)
La Frame Generation utilise les Motion Vectors pour extrapoler une frame intermédiaire entre T et T+1.
- Si le MV est précis, l'image est parfaite.
- Si le MV est absent ou faux (notre situation actuelle), l'interpolation produit des artefacts de "ghosting" massifs.

---

## 4. Plan d'Action Recommandé

### Phase A : Refonte du Core (SH-4 -> PVR)
- Modifier le `TAParser` pour capturer les flux de sommets.
- Implémenter un système de tracking temporel des sommets (Heuristique basée sur l'adresse mémoire ou le Vertex ID généré).

### Phase B : Backend Vulkan
- Activer le MRT (Multiple Render Targets) dans `vulkan_renderer.cpp`.
- Créer un shader de fragment capable d'écrire simultanément dans la couleur et dans le Velocity Buffer.

### Phase C : Intégration SDK
- Intégrer les bibliothèques `FidelityFX FSR3` ou `Streamline (DLSS)`.
- Passer les buffers de Profondeur, Normales et Velocité aux SDK.

## Conclusion
Le succès des Motion Vectors dans Flycast ne dépend pas du shader, mais de la **capacité de l'émulateur à se souvenir de la frame précédente**. En transformant Flycast en moteur "Stateful", on ouvre la porte à une fluidité et une qualité d'image (4K/8K via DLSS/FSR) jusque-là impossibles pour du hardware Dreamcast.

---
*Rédigé par l'Expert Graphics Engineer Vulkan pour l'évolution technologique de Flycast.*
