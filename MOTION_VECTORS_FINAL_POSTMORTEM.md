# Post-Mortem Final : Implémentation des Motion Vectors (Vecteurs de Mouvement) dans Flycast

## Introduction
Ce document clôture les tentatives d'implémentation des Motion Vectors par injection de shader et suivi de sommets dans Flycast (Mars-Avril 2026). Malgré une architecture "Deferred-Ready" fonctionnelle au repos, le calcul de mouvement en temps réel sur du matériel émulé (PVR/SH-4) s'est heurté à des obstacles structurels insurmontables sans une refonte profonde du cœur de l'émulateur.

---

## 1. Ce qui a ÉCHOUÉ (et pourquoi)

### A. La Signature de Sommet (Vertex Hashing)
-   **Tentative** : Utilisation de XXH64 sur les coordonnées (XYZ), UVs et Couleurs pour retrouver le même sommet d'une frame à l'autre.
-   **Résultat** : Écran gris permanent (Gris Neutre 0.5).
-   **Cause** : **Floating Point Drift**. Le processeur SH-4 (ou le compilateur JIT) ne garantit pas une identité binaire stricte des flottants d'une frame à l'autre. Une variation de 0.000001 suffit à invalider le Hash, rendant chaque sommet "nouveau" à chaque frame.

### B. Le Suivi par Slot (Index-Based Tracking)
-   **Tentative** : Comparaison du sommet $N$ de la frame $T$ avec le sommet $N$ de la frame $T-1$.
-   **Résultat** : Chaos "Psyquedélique" (flashs néon, triangles clignotants).
-   **Cause** : **Vertex Order Instability**. L'ordre d'envoi des sommets au Tile Accelerator (TA) change dynamiquement. L'ajout d'un logo sur une voiture, d'un effet de fumée ou le changement de viewport décale tous les index, forçant le shader à comparer des points géométriques n'ayant aucun rapport (ex: roue vs phare).

### C. L'Espace NDC vs Espace Local
-   **Tentative** : Calculer le delta de position après projection (NDC) puis avant projection (Local).
-   **Résultat** : 
    -   **NDC** : Saturations violentes dues aux sauts de matrices de caméra (Projection/View) que Flycast recalcule à chaque frame.
    -   **Local** : Plus stable mais toujours pollué par le bruit CPU et le mélange des listes de rendu (HUD vs 3D).

---

## 2. Le Diagnostic Visuel : "Le Vecteur de Bruit"
L'analyse des screenshots a révélé que le buffer de vélocité était saturé par des valeurs extrêmes (Magenta/Cyan saturé). 
-   **Le Repos est Gris** : Preuve que le shader et le biais (0.5) sont mathématiquement corrects.
-   **Le Mouvement est Chaos** : Preuve que le delta $P(T) - P(T-1)$ est dominé par l'instabilité de la donnée source, et non par le mouvement réel des objets.

---

## 3. Recommandations pour le Prochain Développeur (Phase C)

Pour implémenter des Motion Vectors exploitables pour le **DLSS 3** ou le **FSR 3** dans Flycast, il est impératif d'abandonner l'approche par "injection/suivi" au profit d'une **approche par "transport"** :

1.  **Modification du Core (SH-4/TA)** : 
    -   Il faut étendre la structure `Vertex` dans `ta_ctx.h` dès la capture au niveau du SH-4.
    -   C'est l'émulateur CPU qui doit "marquer" ses sommets avec un identifiant persistant (Vertex ID) ou stocker lui-même la position $T-1$ avant la transformation géométrique.
2.  **G-Buffer MRT (Multiple Render Targets)** :
    -   Ne plus injecter dans le shader de couleur. Utiliser un buffer `R16G16_SFLOAT` dédié.
    -   Isoler strictement les passes de rendu (Opaque, Punch-Through) et exclure totalement le HUD du buffer de vélocité.
3.  **Jittering Correction** :
    -   Pour le TAA/DLSS, il faudra soustraire le "sub-pixel jitter" de la matrice de projection du calcul final de vélocité.

## Conclusion
Le shader de Motion Vectors actuel est un "réceptacle" prêt, mais il attend une source de données stable. Tant que Flycast restera un moteur "stateless" (sans état) au niveau du GPU, le calcul différentiel par sommet sera condamné au bruit.

---
*Rédigé par l'Expert Graphics Engineer Vulkan suite aux tests de stabilité sur Test Drive Le Mans.*
