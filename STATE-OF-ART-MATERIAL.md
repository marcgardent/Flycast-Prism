# Architecture interne de Flycast : Buffer Matériau G-Buffer

## Implémentation réelle (Vulkan)

Le buffer Material est implémenté dans le renderer G-Buffer Vulkan (`core/rend/vulkan/gbuffer/gbuffer_renderer.cpp`).
Il s'agit du **3ème color attachment** du G-Buffer, au format `eR8Uint` (1 octet par pixel).

### Attachments du G-Buffer

| Index | Format | Contenu |
| :--- | :--- | :--- |
| 0 | `eR8G8B8A8Unorm` | Albedo (couleur diffuse) |
| 1 | `eR16G16B16A16Sfloat` | Normales (XYZ en half-float) |
| 2 | `eR8Uint` | **Material ID** (8 bits encodés) |
| depth | `eD32Sfloat` / `eD24UnormS8Uint` | Profondeur |

---

## Encodage du Material ID (8 bits)

Le Material ID est calculé dans `core/rend/vulkan/shaders/vulkan_main.frag` via des constantes de préprocesseur GLSL issues des données PVR natives.

| Bits | Description | Source GLSL |
| :--- | :--- | :--- |
| **7-5** | list_type (0-4) | `cp_AlphaTest`, `IS_TRANSLUCENT` |
| **4** | has_texture | `pp_Texture` |
| **3** | is_gouraud | `pp_Gouraud` |
| **2** | has_bumpmap | `pp_BumpMap` |
| **1-0** | fog_ctrl (0-3) | `pp_FogCtrl` |

### Valeurs list_type (bits 7-5)

| Valeur | list_type | Description |
| :--- | :--- | :--- |
| `0` (000) | `ListType_Opaque` | Géométrie solide |
| `1` (001) | `ListType_Opaque_Modifier` | Volumes shadow sur opaque |
| `2` (010) | `ListType_Translucent` | Alpha blending |
| `3` (011) | `ListType_Translucent_Modifier` | Volumes shadow sur translucide |
| `4` (100) | `ListType_Punch_Through` | Alpha clip binaire (masque) |

---

## Visualisation (ALT+5)

La visualisation est activée via `config::ShowMaterial` (toggle `ALT+5`, reset `ALT+0`).

La passe `MaterialPass` lit le buffer `eR8Uint` via un `usampler2D` et applique un **hash Knuth multiplicatif** pour générer une couleur RGB unique par ID :

```glsl
vec3 hashColor(uint id) {
    uint h = id * 2654435761u;
    return vec3(float((h >> 16) & 255u) / 255.0,
                float((h >> 8)  & 255u) / 255.0,
                float( h        & 255u) / 255.0);
}
```

- **Pixel vide** (matID == 0) → gris sombre `(0.1, 0.1, 0.1)`
- **Chaque combinaison de propriétés** → couleur unique et contrastée

### Exemples de couleurs par type de surface

| Material ID (hex) | list_type | texture | gouraud | bump | fog | Couleur hash |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `0x00` | — | — | — | — | — | Gris sombre (vide) |
| `0x00` | Opaque | non | non | non | off | Hash(0x00) |
| `0x10` | Opaque | oui | non | non | off | Hash(0x10) |
| `0x18` | Opaque | oui | oui | non | off | Hash(0x18) |
| `0x40` | Translucent | non | non | non | off | Hash(0x40) |
| `0x50` | Translucent | oui | non | non | off | Hash(0x50) |
| `0x80` | Punch-Through | non | non | non | off | Hash(0x80) |
| `0x90` | Punch-Through | oui | non | non | off | Hash(0x90) |

> **Note** : Les couleurs exactes dépendent du hash Knuth. Deux surfaces avec le même encodage auront toujours la même couleur, permettant une segmentation sémantique visuelle.

---

## Export EXR du G-Buffer

L'export EXR (déclenché par `ALT+G`) inclut **9 canaux** :

| Canal EXR | Source | Type |
| :--- | :--- | :--- |
| `Albedo.R/G/B` | Attachment 0 | float [0,1] |
| `Normal.X/Y/Z` | Attachment 1 | float [-1,1] |
| `Depth.Z` | Depth attachment | float [0,1] |
| `Material.ID` | Attachment 2 (R8Uint → float) | float [0,1] = matID/255 |
| `SSAO.AO` | SSAOPass buffer (R8Unorm) | float [0,1] |

> Pour retrouver le Material ID entier depuis l'EXR : `matID = round(Material.ID * 255)`.

---

## Pipeline de rendu G-Buffer

```
TA_context (CPU)
    └─► screenDrawer.Draw()          ← G-Buffer pass (Albedo + Normal + MaterialID)
            └─► SSAOPass.Draw()      ← SSAO multiplicatif sur albedo + capture AO brut
                    └─► DoFPass.Draw()    ← Depth of Field (optionnel)
                            └─► MaterialPass.Draw()  ← Visualisation hash color (ALT+5)
```

### Fichiers clés

| Fichier | Rôle |
| :--- | :--- |
| `gbuffer_renderer.cpp` | Orchestration des passes, ExportGBuffer (EXR 9 canaux) |
| `shaders/vulkan_main.frag` | Encodage Material ID 8 bits |
| `shaders/vulkan_top.frag` | Output `layout(location=2) out uint MaterialColor` |
| `shaders/vulkan_ssao.frag` | Calcul AO + écriture `AoRaw` sur location=1 |
| `shaders/vulkan_material.frag` | Hash color visualization |
| `option.h/cpp` | `config::ShowMaterial` (persisté `rend.ShowMaterial`) |
| `sdl.cpp` | `ALT+5` toggle ShowMaterial, `ALT+0` reset |

---

## Points d'attention

* **Modifier Volumes** : Les volumes shadow ont leur propre path. Leur Material ID encode `list_type=1` ou `3`.
* **SSAO buffer séparé** : Le SSAO écrit sur 2 attachments simultanément — l'albedo (blending multiplicatif) et un buffer `R8Unorm` dédié pour l'export EXR.
* **Material ID = 0** : Correspond aux pixels non dessinés (background). À distinguer de `list_type=0` sans texture qui peut aussi produire `0x00`.
* **Extraction EXR** : `Material.ID` est normalisé `matID/255.0`. Multiplier par 255 et arrondir pour retrouver l'ID entier.
