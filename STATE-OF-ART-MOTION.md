# Le Motion Buffer (Velocity Buffer) dans Flycast

## Ce qu'est un Motion Buffer
Dans un moteur moderne, c'est un render target 2D qui stocke pour chaque pixel un vecteur 2D de déplacement en espace écran (en pixels ou en NDC) entre la frame N-1 et la frame N. Il sert principalement au Motion Blur et au TAA (Temporal Anti-Aliasing).

Sur **Flycast**, tu vas devoir le calculer plutôt que le lire depuis le hardware.

---

## Les 3 approches possibles : du plus simple au plus exact

### Approche 1 — Optical Flow purement image (2D)
Tu compares le color buffer (ou l'albedo) de la frame N avec la frame N-1 et tu calcules un optical flow dense pixel par pixel.

* **Faisabilité** : Haute. Entièrement en post-process.
* **Méthode** : `frame[N-1]` + `frame[N]` → Lucas-Kanade / Farneback → texture 2D RG16F.

**Limites sérieuses :**
* Aveugle aux surfaces statiques à texture uniforme.
* Confond mouvement de caméra et d'objet.
* Zones occultées erronées.

---

### Approche 2 — Reprojection temporelle via Depth Buffer (3D)
Tu utilises le depth buffer de ton G-Buffer pour reconstruire la position 3D de chaque pixel.

**La formule de base :**
```
pos_world_N   = unproject(uv, depth_N,   inv(VP_N))
pos_world_N1  = unproject(uv, depth_N1,  inv(VP_N1))
velocity      = project(pos_world_N1, VP_N) - project(pos_world_N, VP_N)
```

**Le problème central sur Flycast :** Absence de matrices View/Projection directes. Le PowerVR2 travaille en coordonnées écran.

**Solutions pour récupérer les matrices :**
* **Heuristique Depth** : Le DC utilise du **W-buffering** (1/W linéaire).
    ```cpp
    // Reconstruction de W (DC stocke 1/W)
    float W = 1.0 / depth_sample;
    // Position en view space (approx)
    vec3 view_pos = vec3((uv * 2 - 1) * W * tan(fov/2), W);
    ```
* **Hook SH4** : Intercepter les vertex avant/après transformation dans `core/hw/sh4/`.

---

### Approche 3 — Motion Buffer géométrique exact (Per-polygon)
L'approche la plus précise : chercher le déplacement de chaque polygone entre frame N-1 et frame N.

**Le défi de l'identité :** Le DC ne donne pas d'ID stable. Il faut une correspondance heuristique :
* **Hash de texture (TCW)** : Les polygones avec la même texture appartiennent probablement au même objet.
* **Proximité spatiale** : Vertices proches entre deux frames.

| Heuristique | Fiabilité | Coût |
| :--- | :--- | :--- |
| Même TCW (texture) | Moyenne | Très Faible |
| TCW + ListType | Meilleure | Faible |
| TCW + Bounding Box | Bonne | Moyen |
| Optical Flow contraint | Très Bonne | Élevé |

---

## Architecture concrète dans Flycast

**Structure de données à ajouter dans `ta_ctx.h` :**
```cpp
struct VertexHistory {
    uint64_t tcw_key;           // hash TCW pour identifier "l'objet"
    std::vector<vec4> verts_N1;  // positions frame précédente
    std::vector<vec4> verts_N;   // positions frame courante
};
```

**Logique de rendu (Shader GLSL) :**
```glsl
// RG16F — vecteur 2D en espace écran normalisé [-1, 1]
layout(location = 4) out vec2 out_velocity;

uniform vec2 u_velocity; // Injecté par draw call via le delta CPU

void main() {
    // ... calculs standards ...
    out_velocity = u_velocity;
}
```

---

## Cas particuliers & Anticipation
* **W-buffer** : La linéarité de 1/W facilite l'interpolation de profondeur.
* **Particules (Translucent)** : Layers superposés = bruit. Utilise la **Material Map** pour filtrer.
* **Texture Scrolling** : Si les UV bougent mais pas la géométrie. L'Optical Flow est ici complémentaire.

## Recommandation par étapes
1.  **Phase 1 (Caméra)** : Estimer le mouvement global via le delta des vertices statiques.
2.  **Phase 2 (Segmentation TCW)** : Grouper par hash texture pour un motion blur par "objet".
3.  **Phase 3 (Raffinement)** : Utiliser la géométrie comme *prior* pour un Optical Flow GPU.