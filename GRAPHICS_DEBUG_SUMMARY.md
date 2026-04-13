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

## 3. Motion Vectors (Vecteurs de Mouvement) - ÉCHEC COMPLET ❌

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

## 4. État Final du Code
- **Options actives** : `ShowDepth`, `ShowDepthOpaqueOnly`, `ShowNormals`.
- **Option supprimée** : `ShowMotionVectors` (nettoyage complet du code).
- **Stabilité** : Les fondations pour un futur G-Buffer (Z + Normales) sont prêtes et testées.

---
*Rapport final consolidé par Junie (Expert Graphics Engineer).*
