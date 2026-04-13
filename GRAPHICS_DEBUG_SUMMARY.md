# Synthèse des Travaux de Debug Graphique (Avril 2026)

Ce document résume les tentatives, succès et échecs concernant l'implémentation d'outils de visualisation graphique (Z-Map, Normal Map) et de calcul de mouvement (Motion Vectors) dans Flycast.

---

## 1. Z-Map (Visualisation de la Profondeur) - SUCCÈS ✅

### Approche retenue : Injection Shader
- **Méthode** : Injection d'une constante `ShowDepth` dans le shader de fragment.
- **Détails techniques** : Affichage de `gl_FragDepth` (profondeur logarithmique native Flycast).
- **Optimisation** : Ajout d'un seuil alpha (0.2) sur les polygones translucides pour isoler la géométrie "propre" (Punch-Through).
- **Résultat** : Visualisation stable et performante, essentielle pour le debugging de la visibilité PVR.

---

## 2. Normal Map (Visualisation des Normales) - SUCCÈS ✅

### Approche retenue : Reconstruction & Propagation
- **Méthode** : Utilisation des normales par vertex pour Naomi 2, et reconstruction via dérivées partielles (`dFdx/dFdy`) pour la Dreamcast standard.
- **Espace** : Travail dans l'**Espace Vue (View Space)** pour la compatibilité avec les effets de post-process futurs (SSAO).
- **Cohérence** : Partage la même logique de filtrage alpha (0.2) que le Z-Map.
- **Bump Mapping** : Support de la visualisation des normales perturbées par le bump mapping original du PVR.

---

## 3. Gestion des Ombres (Modifier Volumes) - RÉSULTAT OPTIMAL ✅

### Échecs des approches initiales :
- **Approche par bit Shadow (Shader)** : Tentative de filtrer via le flag matériel `Shadow` (PCW). Échec car ce bit est souvent utilisé pour des objets normaux "recevant" des ombres, ce qui masquait tout le décor.
- **Approche par Discard/Noir** : Produisait des artefacts ou la disparition complète des voitures/décors car le pipeline PVR mélange les états de manière complexe.

### Solution finale retenue :
- **Méthode** : Désactivation pure et simple des **Modifier Volumes (ModVols)** au niveau de la boucle de rendu (`drawer.cpp`).
- **Logique** : Les ModVols (ombres portées) utilisent leur propre shader et passent après le rendu géométrique. En les bloquant quand `ShowNormals` est actif, on préserve un Normal Map pur sans pollution d'ombre.
- **Impact Z-Map** : Le Z-Map reste conforme car les ombres ne modifient pas la profondeur de manière destructive pour le debug.

---

## 4. Motion Vectors (Vecteurs de Mouvement) - ÉCHEC COMPLET ❌

### Pourquoi l'approche par shader/tracking a échoué :
- **Transformation CPU (SH-4)** : Le PVR reçoit des sommets déjà transformés. Le shader ne voit pas l'historique $T-1$.
- **Instabilité de l'ordre des sommets** : L'ordre d'envoi au Tile Accelerator change à chaque frame, rendant le suivi par index impossible.
- **Floating Point Drift** : Le hashage des sommets (XYZ/UV) échoue car le SH-4/JIT produit des variations infinitésimales de flottants entre deux frames, invalidant l'identité binaire des sommets.
- **Résultat** : Bruit visuel intense ou écran gris plat, sans aucune donnée de mouvement exploitable.

### Recommandations pour le futur :
Pour implémenter les Motion Vectors (DLSS/FSR3), il est impératif d'abandonner l'injection de shader au profit d'une **approche par transport** :
1. Modifier le Core (SH-4/TA) pour étendre la structure `Vertex` dès la capture.
2. Transporter un **Vertex ID unique** ou la position $T-1$ calculée côté CPU.
3. Passer à une architecture **G-Buffer MRT** réelle (R16G16_SFLOAT dédié).

---

## 5. Raccourcis Clavier & Contrôle - NOUVEAU ✅
- **Alt+0** : Retour au rendu natif (désactivation des debug maps).
- **Alt+1** : Basculer vers le **Z-Map** (Profondeur).
- **Alt+2** : Basculer vers le **Normal Map** (Normales).
- **Mécanisme** : Implémenté dans la boucle d'événements SDL (`sdl.cpp`), déclenchant la recompilation à la volée des pipelines Vulkan via un hash d'options global.

---

## 6. État Final du Code
- **Options actives** : `ShowDepth`, `ShowNormals`.
- **Option supprimée** : `ShowMotionVectors` (nettoyage complet du code).
- **Stabilité** : Les fondations pour un futur G-Buffer (Z + Normales) sont prêtes et testées, sans pollution par les volumes d'ombre.

---
*Rapport final consolidé par Junie (Expert Graphics Engineer).*
