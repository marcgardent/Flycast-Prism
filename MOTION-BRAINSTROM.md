Félicitations pour la semantic map ! C'est une vraie fondation.

Maintenant creusons le problème velocity. Tu touches quelque chose de fondamental.

---

## Pourquoi la capture caméra a échoué — le vrai diagnostic

### Le malentendu "2D vs 3D"

Quand on dit que le DC envoie des vertices "déjà en 2D", ce n'est pas tout à fait exact — et c'est là que la confusion vient. Voici ce qui se passe réellement :

```
Jeu (SH4 CPU)
    └── transforme les vertices monde → clip space (matrice MVP)
    └── envoie au Tile Accelerator des vertices en coordonnées homogènes (X, Y, 1/W)
                                                                              ↑
                                              PAS Z/W classique — c'est 1/W (W-buffer)
Flycast voit ça côté TA input
    └── X, Y en coordonnées écran
    └── 1/W comme profondeur
```

Donc techniquement tu as bien une **profondeur 3D** (1/W), mais tu n'as **pas les matrices** qui ont produit cette transformation. Le jeu les a calculées dans son propre code SH4, consommées, et jetées. Flycast ne les a jamais vues.

### Pourquoi le viewport Vulkan n'aide pas

C'est une question très pertinente. Le viewport Vulkan dans Flycast est **en aval** du problème :

```
SH4 calcule MVP          ← matrices ici, invisibles pour Flycast
    ↓
Vertices clip space      ← Flycast les voit ici (X, Y, 1/W)
    ↓
Tile Accelerator
    ↓
Rasterizer OpenGL/Vulkan ← viewport ici, mais les matrices sont déjà "consommées"
```

Le viewport Vulkan reçoit des vertices **déjà projetés**. Il fait de la rastérisation sur des coordonnées écran, pas de la transformation 3D. Donc même en Vulkan, tu n'as pas accès à la matrice de caméra.

C'est ça le mur fondamental.

---

## Brainstorm : solutions pour le mouvement de caméra

### Piste 1 — Reconstruction de matrice par résolution de système linéaire

C'est l'approche mathématiquement correcte. Si tu as suffisamment de correspondances **position 3D monde → position écran**, tu peux résoudre la matrice MVP par moindres carrés.

**Problème :** tu n'as pas les positions monde. Tu as les positions écran et 1/W.

Mais tu peux reformuler : si tu as le **même vertex** à deux frames consécutives, et que tu sais qu'il appartient à de la géométrie statique, alors :

```
frame N   : pixel (x1, y1), depth d1
frame N-1 : pixel (x2, y2), depth d2
→ delta (x2-x1, y2-y1) = mouvement de caméra projeté sur ce point
```

Le vrai problème reste : **comment identifier que c'est le même vertex statique** entre deux frames. On revient au problème de tracking.

---

### Piste 2 — Gyroscope virtuel depuis le depth buffer (sans matrices)

C'est une approche qui évite complètement les matrices. L'idée : le **champ de vecteurs de mouvement d'une caméra rigide** dans une scène statique a une structure mathématique très particulière — il peut être décomposé en une composante de **translation** et une composante de **rotation**, et ces deux composantes ont des signatures géométriques distinctes dans l'image.

En pratique : tu prends le depth buffer frame N et N-1, tu calcules un optical flow dense dessus (ou sur l'albedo), et tu **fites un modèle de mouvement de caméra rigide** sur ce flow plutôt que d'utiliser le flow brut.

```
optical_flow_brut (plein d'artefacts)
    └── RANSAC ou least squares fit
    └── modèle : flow = f(tx, ty, tz, rx, ry, rz, depth)
    └── résultat : 6 DOF camera motion estimé
    └── → velocity buffer propre reconstruit depuis ce modèle
```

C'est ce que font les algorithmes de **Visual Odometry** (VO) en robotique. Le depth buffer que tu as est exactement ce qu'un VO a besoin.

**Avantage majeur :** ça sépare automatiquement le mouvement de caméra du mouvement d'objet — les pixels qui ne s'expliquent pas par le modèle de caméra rigide sont des objets en mouvement.

**Librairies de référence :** COLMAP, ORB-SLAM, ou une implémentation RANSAC homemade assez simple.

---

### Piste 3 — Hooker le bus SH4 → TA pour intercepter les matrices

C'est la solution "chirurgicale" dans le code Flycast. Le jeu envoie ses vertices transformés via les **Store Queues** du SH4 vers le Tile Accelerator. Juste avant cette transformation, les vertices sont en coordonnées monde. Juste après, ils sont en clip space.

Dans `core/hw/pvr/ta.cpp` ou `core/hw/sh4/`, tu peux théoriquement intercepter **les vertices pre-transform** si tu hooks au bon endroit dans le cycle d'émulation.

Mais c'est très invasif, fragile selon les jeux, et certains jeux calculent leurs matrices différemment (certains font tout en assembleur SH4 optimisé).

---

### Piste 4 — Feature matching entre frames (SIFT/ORB-like)

Au lieu d'un optical flow dense, tu cherches des **keypoints stables** entre frame N-1 et N, tu trouves leurs correspondances, et tu en déduis la transformation de caméra.

```
frame N-1 albedo → keypoints {p1, p2, p3...}
frame N   albedo → keypoints {q1, q2, q3...}
correspondances  → homographie H ou matrice essentielle E
→ décomposition → R (rotation caméra) + t (translation caméra)
→ velocity buffer reconstruit
```

Avec le depth buffer en plus, tu passes de l'homographie 2D à une **estimation 3D complète** (Perspective-n-Point).

**Robustesse :** bien meilleure que l'optical flow brut sur des frames DC parce que tu ignores les zones sans texture (fond uni, ciel, etc.) et tu te concentres sur les features saillants.

---

### Piste 5 — Le fallback pragmatique : détection de mouvement global par phase correlation

C'est la solution la plus simple et la plus rapide à implémenter. La **phase correlation** en fréquentiel donne la translation globale entre deux images en O(N log N) via FFT. C'est utilisé en stabilisation vidéo.

```cpp
// FFT de frame N et N-1
// Produit croisé normalisé dans le domaine fréquentiel
// IFFT → pic de Dirac à la position (dx, dy)
// → translation globale de caméra en pixels
```

Ça ne donne que la **translation 2D écran**, pas la rotation ni la profondeur. Mais c'est un signal robuste et ultra-rapide. Suffisant pour un motion blur de caméra convaincant dans 80% des cas (notamment les déplacements latéraux et les travellings).

---

## Sur le tracking d'objets par texture — pourquoi ça clignote

Tu as raison que c'est instable. Voici les causes précises :

### Cause 1 — Texture sharing
Une même texture est utilisée par des polygones d'objets différents. Le carrelage du sol, les murs répétitifs — même TCW, géométrie complètement différente. Ton heuristique regroupe des choses qui n'ont rien à voir.

### Cause 2 — Discontinuité de visibility
Un polygone disparaît (occulté, frustum culled) à la frame N et réapparaît à la frame N+2. Ton tracker perd la correspondance et recalcule un delta depuis une position complètement différente → spike de vélocité.

### Cause 3 — Le W-buffer et la précision
Les valeurs 1/W sont non-linéaires dans leur distribution. Un petit delta de 1/W près de la caméra = grand déplacement, loin = petit déplacement. Si tu calcules le centroid en espace écran sans pondérer par la profondeur, le mouvement calculé est faux en magnitude.

### Cause 4 — Geometry instancing invisible
Certains jeux DC dessinent le même objet plusieurs fois par frame (miroirs, doubles passes pour effects). Même TCW, positions différentes, pas un mouvement réel.

---

## Architecture recommandée : pipeline hybride

Voici comment je ferais cohabiter tout ça :

```
┌─────────────────────────────────────────────────────┐
│                   VELOCITY BUFFER                    │
│                                                      │
│  Layer 1 : Camera motion (global)                   │
│  ┌──────────────────────────────────────────────┐   │
│  │  Phase correlation ou Visual Odometry        │   │
│  │  → vecteur global (tx, ty) ou 6DOF           │   │
│  │  → appliqué à TOUS les pixels static         │   │
│  └──────────────────────────────────────────────┘   │
│                         +                           │
│  Layer 2 : Object motion (par semantic region)      │
│  ┌──────────────────────────────────────────────┐   │
│  │  Semantic map (tu l'as déjà !)               │   │
│  │  → masque les Translucent/Modifier           │   │
│  │  → sur les Opaque uniquement                 │   │
│  │  → optical flow contraint par depth          │   │
│  │  → RANSAC pour rejeter les outliers          │   │
│  └──────────────────────────────────────────────┘   │
│                         +                           │
│  Layer 3 : Particles / FX                           │
│  ┌──────────────────────────────────────────────┐   │
│  │  Translucent+Additive → velocity = camera    │   │
│  │  (les particules bougent avec la caméra      │   │
│  │   ou sont ignorées dans le motion blur)      │   │
│  └──────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

Ta semantic map devient le **masque de routage** entre les trois layers. C'est pour ça que l'ordre d'implémentation que tu as suivi était le bon.

---

## Recommandation concrète pour le fallback caméra

Pour débloquer rapidement, voici ce que je ferais dans l'ordre :

**Étape 1 — Phase correlation sur le depth buffer** (pas le color). Le depth est moins bruité par les effets visuels, plus stable. Donne `(dx, dy)` en pixels en 2-3 heures de code.

**Étape 2 — Filtrer avec la semantic map** : ne calcule la phase correlation que sur les pixels `Opaque` non-`Modifier`. Les effets et les transparents faussent le signal.

**Étape 3 — Smooth temporel** : moyenne glissante sur 3-5 frames pour éliminer les spikes. Le mouvement de caméra est physiquement lisse.

**Étape 4 — Valider sur Shenmue** : jeu parfait pour tester, caméra lente et prévisible, beaucoup de géométrie opaque stable.

---

## Ce qui reste fondamentalement non-résolu

Honnêtement, deux problèmes n'ont pas de solution propre sans accès aux matrices :

La **rotation de caméra pure** (pan horizontal/vertical sans translation) est très difficile à distinguer d'un mouvement d'objet en 2D écran. La phase correlation ne la voit pas bien. Il faut du VO avec depth pour la récupérer.

La **profondeur de champ du mouvement** — savoir si un objet bouge vite parce qu'il est proche ou parce qu'il se déplace vraiment vite — est impossible sans la matrice de projection. Tu peux l'approximer avec 1/W mais avec une erreur systématique sur les objets proches.

Ces deux cas sont rares dans les jeux DC typiques (caméra souvent semi-fixe, mouvement doux), donc en pratique ton fallback sera très acceptable visuellement.

Tu veux qu'on code la phase correlation sur depth buffer en premier ?