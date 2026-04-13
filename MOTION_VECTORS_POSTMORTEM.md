# Post-Mortem : Échec de l'implémentation des Motion Vectors (MV) dans Flycast

## Introduction
L'objectif était d'implémenter et de visualiser les vecteurs de mouvement (Motion Vectors) dans le pipeline Vulkan de Flycast pour permettre de futurs effets de post-process (TAA, Motion Blur). Après plusieurs itérations, l'implémentation a été déclarée comme un échec technique pour l'architecture actuelle de l'émulateur, particulièrement sur le rendu Dreamcast standard.

## Pourquoi cela a échoué (Analyse Technique)

### 1. La Transformation CPU (SH-4) sur Dreamcast
C'est la cause racine majeure. Contrairement aux moteurs modernes où le GPU transforme les sommets à partir de l'espace objet vers l'espace écran via des matrices (MVP), le matériel Dreamcast (PVR) reçoit souvent des sommets **déjà transformés** par le CPU (SH-4).
- **Conséquence** : Dans le Vertex Shader Vulkan, la matrice `ndcMat` est souvent une simple identité ou une matrice de mise à l'échelle statique.
- **Problème** : Pour calculer un mouvement, il faut comparer $P(T)$ et $P(T-1)$. Comme le Vertex Shader ne voit que le résultat final du calcul CPU à l'instant $T$, il n'a aucune information sur la position du sommet à $T-1$.

### 2. Statelessness du Flux de Sommets
Le pipeline Vulkan de Flycast est "sans état" vis-à-vis de la frame précédente au niveau des attributs de sommets.
- Il n'y a pas d'identifiant unique (Vertex ID) persistant entre les frames qui permettrait de corréler un sommet actuel avec sa position précédente stockée dans un buffer.
- Sans cette corrélation, même en stockant les données, le shader ne sait pas quel "ancien" sommet correspond au "nouveau".

### 3. Les Limites de la Reconstruction par Matrices
L'approche retenue (Alternative A : Reconstruction par Matrices) supposait que le mouvement provenait principalement de la caméra.
- **Naomi 2** : Cela fonctionne théoriquement car elle utilise un T&L matériel avec des matrices explicites.
- **Dreamcast** : Comme expliqué au point 1, la matrice est "cuite" dans les coordonnées envoyées au GPU. La reconstruction matricielle ne produit donc qu'un delta nul (ou constant), ce qui explique l'écran "gris plat" (0.5 de biais).

### 4. L'Inutilité du Jitter de Debug
L'injection d'un "jitter" (gigue) artificiel de 0.01 dans le calcul de la position précédente a été tentée pour prouver que le shader était actif. Bien que cela ait coloré l'écran (sortant du gris neutre), cela n'a apporté aucune donnée de mouvement réelle ou exploitable, confirmant que le calcul sous-jacent était aveugle au mouvement réel du jeu.

## Obstacles Architecturaux Insurmontables
Pour réussir les Motion Vectors sur Flycast, il faudrait :
1. **Modifier le Core de l'émulateur** pour capturer les positions des sommets calculées par le SH-4 à la frame précédente et les injecter comme un nouvel attribut de sommet (`in_prev_pos`) dans le shader Vulkan.
2. **Gérer le Skinning CPU** : Comme les animations sont calculées sur le CPU, chaque sommet bouge indépendamment. Seule l'injection de la position précédente par sommet (et non par matrice) permettrait de capturer le mouvement des personnages.

## Conclusion
L'implémentation via shader seul (injection de constantes et matrices globales) est **suffisante pour le Z-Map et les Normales** (qui sont des propriétés instantanées du fragment), mais elle est **fondamentalement insuffisante pour les Motion Vectors** (qui sont une propriété temporelle différentielle).

Une architecture "Deferred-Ready" avec un G-Buffer de vitesse nécessiterait une refonte profonde de la communication entre le CPU (SH-4) et le backend Vulkan pour transporter les données historiques de géométrie.

---
*Document rédigé par l'Expert Graphics Engineer Vulkan suite à l'analyse de l'échec de la feature.*
