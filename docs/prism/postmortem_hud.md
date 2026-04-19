# Postmortem — Debug HUD Buffer Noir

> **Date** : 2026-04-18
> **État actuel** : Baseline stable (albedo OK, HUD noir — non résolu)

---

## 1. Faits Établis ✅ (Validés par tests visuels)

| # | Test | Résultat | Conclusion |
|---|------|----------|------------|
| 1 | `HUDColor = vec4(1,0,0,1)` | 🔴 Rouge | Le fragment s'exécute, `isHUD > 0.0` fonctionne, attachment 4 reçoit des données |
| 2 | `HUDColor = vec4(gl_FragCoord.xy/640, 0, 1)` | 🌈 Gradient | Confirme #1, indépendamment des varyings vertex |
| 3 | `HUDColor = color` | ⚫ Noir | La couleur calculée par le pipeline standard est (0,0,0) |
| 4 | `HUDColor = vec4(vtx_base.rgb, 1)` | ⚫ Noir | `vtx_base` = (0,0,0) après vertex shader |
| 5 | `HUDColor = vec4(vtx_offs.rgb, 1)` | ⚫ Noir | `vtx_offs` = (0,0,0) après vertex shader |
| 6 | `HUDColor = vec4(vtx_uv.z, z*0.1, z*0.01, 1)` | ⚫ Noir | `vtx_uv.z ≈ 0` même avec amplification ×100 |
| 7 | `HUDColor = textureProj(tex, vtx_uv)` (pp_Texture=1) | ⚫ Noir | Texture coords corrompues (0/0) |
| 8 | `HUDColor = vec4(1,0,1,1)` (pp_Texture=0) | 🟣 Magenta | Seuls les polygones NON-texturés apparaissent |
| 9 | Albedo sans HUD | ✅ Parfait | La discrimination isHUD (DepthMode >= 6) fonctionne |

## 2. Cause Racine Identifiée

### Le mécanisme de perspective-correct Gouraud détruit les données HUD

Dans le vertex shader (`vulkan_main.vert`), quand `DIV_POS_Z != 1` :

```glsl
// vpos.z ≈ 1e-30 pour les polygones HUD (DepthMode=Always)
vtx_base *= vpos.z;      // → ≈ 0  (couleur détruite)
vtx_offs *= vpos.z;      // → ≈ 0  (offset détruit)
vtx_uv.xy *= vpos.z;     // → ≈ 0  (UVs détruits)
vtx_uv.z = vpos.z;       // → ≈ 0  (diviseur ≈ 0)
```

Dans le fragment shader (`vulkan_main.frag`) :
```glsl
color /= vtx_uv.z;               // → 0/0 = NaN → noir
textureProj(tex, vtx_uv);        // → 0/0 = NaN → texture noire
```

**`vpos.z` n'est PAS exactement 0.0** mais très proche (~1e-30). Le test `!= 0.0` ne le capte pas. Le test `abs(z) > seuil` nécessite un seuil correct mais les z de scène descendent aussi très bas.

## 3. Tentatives de Fix et Résultats

### 3.1. Guard fragment shader (`abs(vtx_uv.z) > 1e-6`)
- **Résultat** : ❌ Insuffisant
- **Raison** : Le vertex shader a DÉJÀ multiplié vtx_base par ~0. Garde le 0 intact. Et les UVs sont aussi corrompus.

### 3.2. Guard vertex shader — seuil `abs(vpos.z) > 1e-6`
- **Résultat** : ❌ Casse tout le rendu (all buffers empty)
- **Cause réelle** : Ce n'était PAS le guard lui-même mais le push constant range (36 bytes) qui était trop petit pour les 48 bytes envoyés (avec le padding ajouté). Le guard fonctionne probablement.

### 3.3. Guard vertex shader — seuil `0.0001`
- **Résultat** : ✅ HUD rouge, ❌ mais casse la scène
- **Raison** : Seuil trop élevé — capture aussi les polygones de scène normaux avec des petits z

### 3.4. Guard vertex shader — seuil `1e-7`
- **Résultat** : ✅ HUD rouge, ❌ mais casse la scène
- **Raison** : Seuil encore trop élevé — certains polygones de scène ont z < 1e-7

### 3.5. Guard vertex shader — `vpos.z != 0.0`
- **Résultat** : ❌ HUD toujours noir
- **Raison** : vpos.z n'est pas exactement 0.0 (quelque chose comme 1e-30), le test d'égalité ne marche pas

### 3.6. isHUD en push constant dans le vertex shader
- **Résultat** : Non testable proprement (cassé par accumulation de changements)
- **Approche** : La plus prometteuse, mais nécessite une implémentation propre

## 4. Correctifs Latéraux Découverts

### 4.1. Push Constant Range trop petit ⚠️
- **pipeline.h** : range = 36 bytes
- **Données envoyées** : 9 floats × 4 = 36 bytes (original sans padding)
- **Layout GLSL** : `float isHUD` + `vec4 clipTest` (aligné à 16) → 48 bytes théoriques
- **Statut** : Le driver semble tolérer le mismatch car `pp_ClipInside == 0` pour la plupart des polygones → clipTest n'est jamais lu → pas de crash visible.
- **Impact** : `trilinearAlpha`, `palette_index`, `velocity` lisent des offsets décalés → valeurs potentiellement incorrectes.

### 4.2. Alignement Push Constant GLSL vs C++ ⚠️
Le layout GLSL avec `float isHUD` suivi de `vec4 clipTest` insère un padding implicite de 12 bytes (std430 vec4 alignment = 16). Le C++ envoie les données sans ce padding.

| Champ | Offset GLSL (std430) | Offset C++ |
|-------|---------------------|------------|
| isHUD | 0 | 0 ✅ |
| clipTest.x | **16** | **4** ❌ |
| clipTest.y | 20 | 8 ❌ |
| clipTest.z | 24 | 12 ❌ |
| clipTest.w | 28 | 16 ❌ |
| trilinearAlpha | 32 | 20 ❌ |
| palette_index | 36 | 24 ❌ |
| velocity.x | 40 | 28 ❌ |
| velocity.y | 44 | 32 ❌ |

**`isHUD` est correct** (offset 0 des deux côtés). Tous les autres champs sont **mal alignés**. Cela n'est pas visible car `pp_ClipInside == 0` pour la plupart des polygones.

## 5. État Actuel de la Baseline

Tous les fichiers sont revenus à l'état stable :
- `vulkan_main.vert` : Original (pas de modification)
- `vulkan_main.frag` : Routing HUD basique (`HUDColor = color`), alpha discard bypass pour HUD
- `vulkan_top.frag` : Layout original sans padding
- `drawer.cpp` : 9 floats sans padding
- `pipeline.h` : 36 bytes, fragment only

## 6. Prochaines Étapes — Solution Recommandée

### Étape 1 : Corriger l'alignement push constant (prerequis)
1. Ajouter 3 floats de padding dans le C++ APRÈS isHUD → 12 floats = 48 bytes
2. Ajouter les 3 `_pad` dans le GLSL
3. Changer le range dans `pipeline.h` à 48 bytes
4. **Tester** : albedo doit rester parfait

### Étape 2 : Passer isHUD au vertex shader
1. Changer le stage flag : `eFragment | eVertex`
2. Déclarer `layout(push_constant) uniform pushBlock { float isHUD; }` dans le vertex shader
3. Guard : `if (pushConstants.isHUD > 0.0) { vtx_uv.z = 1.0; } else { vtx_base *= vpos.z; ... }`
4. **Tester** : albedo parfait + HUD visible

### Étape 3 : Valider le rendu final
1. Retirer les debug dans le fragment shader
2. Vérifier le composite ALT+0
3. Vérifier l'export EXR

> **Point clé** : L'étape 1 DOIT être faite et validée seule avant l'étape 2. L'erreur passée a été de cumuler trop de changements simultanés.
