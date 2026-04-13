# Recommandations pour l'implémentation des Motion Vectors (Vecteurs de Mouvement) dans Flycast

## Introduction
Le but est d'extraire et de visualiser les vecteurs de mouvement à l'écran. Ces vecteurs représentent le déplacement de chaque pixel entre la frame précédente ($T-1$) et la frame actuelle ($T$). Ils sont essentiels pour le post-process moderne comme le TAA (Temporal Anti-Aliasing), le Motion Blur ou le DLSS.

## Fondamentaux Mathématiques
Le vecteur de mouvement $\vec{v}$ pour un point $P$ est défini par :
$$\vec{v} = P_{NDC}(T) - P_{NDC}(T-1)$$

Cependant, pour être exploitable par un shader de post-process (qui travaille en espace écran), on utilise souvent :
$$\vec{v} = \text{UV}_{T} - \text{UV}_{T-1}$$
Où $\text{UV}$ sont les coordonnées de texture normalisées $[0, 1]$ calculées à partir des positions NDC.

## Architecture Proposée

### 1. Stockage de l'état temporel
Flycast utilise un `rend_context` par frame. Pour calculer le mouvement, nous devons conserver les matrices de transformation de la frame précédente de manière persistante.

- **Centralisation dans VulkanContext** : Pour éviter que les multiples passes de rendu (`Drawer::Draw`) ne corrompent l'état précédent au sein d'une même frame, le stockage doit être centralisé dans le `VulkanContext`.
- **Mécanisme de Double-Buffering** : Le `VulkanContext` maintient `currNdcMat` et `prevNdcMat`. La permutation ne se produit qu'une seule fois par appel à `Present()` ou `NewFrame()`.

### 2. Débat sur les Alternatives de Calcul

#### Alternative A : Reconstruction par Matrices (Approche retenue)
- **Principe** : Stocker $M_T$ et $M_{T-1}$. Dans le vertex shader, projeter le sommet avec les deux matrices.
- **Avantages** : Précis pour le mouvement de caméra. Facile à implémenter sans modifier les structures de sommets.
- **Inconvénients** : Ne capture pas le mouvement propre des objets (skinning CPU) à moins de stocker les positions précédentes des sommets.

#### Alternative B : G-Buffer Persistant (Velocity Buffer)
- **Principe** : Écrire la vitesse calculée dans un attachment dédié (MRT).
- **Avantages** : Standard dans les moteurs modernes. Permet une utilisation directe par les shaders de post-process.
- **Inconvénients** : Nécessite une architecture Deferred/MRT que Flycast n'a pas encore nativement.

#### Alternative C : Reconstruction à partir du Depth Buffer
- **Principe** : Utiliser la position écran actuelle et la profondeur pour reconstruire la position monde, puis la reprojeter avec $M_{T-1}$.
- **Avantages** : Ne nécessite pas de passer des données supplémentaires par sommet.
- **Inconvénients** : Très coûteux en calculs dans le fragment shader. Problèmes de précision sur les bords d'objets.

### 3. Pipeline de Shaders (Détails)
Le calcul doit se faire au niveau du **Vertex Shader** pour être efficace, puis être interpolé vers le **Fragment Shader**.

#### Vertex Shader (Vulkan)
- **Entrées** : `in_pos` (position du sommet).
- **Uniforms** : `ndcMat` (actuelle) et `prevNdcMat` (précédente).
- **Calcul** :
  ```glsl
  vec4 currPos = uniformBuffer.ndcMat * in_pos;
  vec4 prevPos = uniformBuffer.prevNdcMat * in_pos;
  
  // Conversion en coordonnées de texture (0,1)
  vtx_motion.xy = (currPos.xy / currPos.w) * 0.5 + 0.5;
  vtx_motion.zw = (prevPos.xy / prevPos.w) * 0.5 + 0.5;
  ```

#### Fragment Shader
Le vecteur de mouvement brut est : `motion = vtx_motion.xy - vtx_motion.zw`.
Pour la visualisation (Debug) :
- On peut encoder la direction dans les canaux R et G.
- `FragColor.rg = motion.xy * gain + 0.5;`
- `FragColor.b = 0.0;`
- `FragColor.a = 1.0;`

## Défis Techniques

### 1. Naomi 2 et Matrices par Objet
Pour Naomi 2, les objets ont leurs propres matrices de transformation (`mvMat`, `projMat`). L'implémentation devra également suivre ces matrices ou utiliser une approche simplifiée si les objets sont statiques par rapport à la caméra.

### 2. Changements de Contexte (Warp/Teleport)
Si la caméra se téléporte, les vecteurs de mouvement seront invalides pour une frame. Il faut prévoir un mécanisme pour "reset" les vecteurs (ex: mettre `prevNdcMat = ndcMat`).

### 3. Objets Animés (Skinning)
Le Dreamcast/Naomi n'utilise pas de skinning GPU standard (tout est calculé CPU ou via des listes de polygones). Le vecteur de mouvement capturera le mouvement de la caméra, mais pour le mouvement propre des personnages (bras qui bouge), cela nécessiterait de suivre les positions des sommets à $T-1$ au niveau du CPU, ce qui est complexe à intégrer dans le pipeline PVR actuel sans modifications majeures des structures de données.

## État de l'implémentation (Échec du Debug)
L'implémentation via shader seul est un **échec** pour le rendu Dreamcast standard pour les raisons détaillées dans `MOTION_VECTORS_POSTMORTEM.md`.

### Pourquoi l'approche matricielle a échoué :
- Sur Dreamcast, la matrice `ndcMat` est statique car le CPU transforme les sommets.
- Le shader ne connaît pas la position précédente des sommets.
- La Naomi 2 (T&L matériel) est la seule candidate pour un mouvement partiel, mais l'architecture globale ne permet pas de synchroniser ces données proprement.

### Recommandations pour le futur :
- Abandonner l'approche par injection de shader simple.
- Implémenter une capture des positions de sommets au niveau du core SH-4 pour transporter les deltas temporels.
- Passer à une architecture G-Buffer réelle (MRT) avec un buffer de vélocité dédié.

---
*Document rédigé par l'Expert Graphics Engineer Vulkan.*
