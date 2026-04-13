# Modifications Clés par rapport à Master (Branche zmap-debugging)

Ce document détaille les changements techniques majeurs effectués sur la branche `zmap-debugging` par rapport à la branche `master`. Ces modifications ont permis l'implémentation des outils de debug graphique (**Z-Map** et **Normal Map**).

---

## 1. Noyau & Configuration (`core/cfg/`)
L'ajout de nouvelles options globales permet de contrôler l'activation des modes de debug.

- **`option.cpp` / `option.h`** :
    - Ajout de `ShowDepth` (bool) : Active la visualisation de la profondeur.
    - Ajout de `ShowDepthOpaqueOnly` (bool) : Filtre les objets translucides.
    - Ajout de `ShowNormals` (bool) : Active la visualisation des normales.

## 2. Pipeline de Rendu Vulkan (`core/rend/vulkan/`)
C'est ici que réside la logique de visualisation et de transport des données géométriques.

- **`shaders.cpp` / `shaders.h`** :
    - **Vertex Shader** : 
        - Ajout de l'attribut d'entrée `in_normal` (location 4) pour récupérer les normales par sommet (Naomi 2).
        - Export de `vtx_pos` et `vtx_normal` vers le Fragment Shader pour les calculs de reconstruction.
    - **Fragment Shader** :
        - Injection des constantes `ShowDepth` et `ShowNormals`.
        - **Z-Map** : Utilisation de `gl_FragDepth` pour afficher la profondeur logarithmique native. Ajout d'un `discard` sur l'alpha (< 0.2) pour nettoyer les polygones translucides bruyants.
        - **Normal Map** : 
            - Si les normales par sommet sont présentes, elles sont utilisées.
            - Sinon, reconstruction via les dérivées partielles : `normalize(cross(dFdx(vtx_pos), dFdy(vtx_pos)))`.
            - Support du **Bump Mapping** PVR original en visualisant les normales perturbées.
    - **ShaderManager** : Mise à jour du hash des shaders pour inclure les nouveaux états de debug, forçant la recompilation des pipelines lors du changement de mode.

- **`pipeline.cpp` / `pipeline.h`** :
    - Extension de `FragmentShaderParams` pour inclure `showDepth`, `showNormals` et `isTranslucent`.
    - Mise à jour de la configuration des attributs de sommet (`VkVertexInputAttributeDescription`) pour inclure l'emplacement de la normale.

- **`drawer.cpp`** :
    - **Gestion des Ombres** : Ajout d'une condition critique pour désactiver le rendu des **Modifier Volumes (ModVols)** lorsque le mode Normal Map est actif. Cela évite que les volumes d'ombre ne "polluent" les normales de la scène géométrique.

## 3. Entrées & Interface (`core/sdl/`)
Intégration de raccourcis clavier pour une utilisation fluide en cours de jeu.

- **`sdl.cpp`** :
    - Interception des combinaisons de touches avec `ALT` :
        - **Alt + 0** : Désactive tout (`ShowDepth = false`, `ShowNormals = false`).
        - **Alt + 1** : Active le **Z-Map**.
        - **Alt + 2** : Active le **Normal Map**.

## 4. Données Géométriques (`core/hw/pvr/`)
- **`ta_vtx.cpp`** :
    - Modification du parsing des sommets pour extraire et transmettre les données de normales si elles sont présentes dans le flux TA (Tile Accelerator).

---
*Ce récapitulatif a été généré par Junie suite à l'analyse du diff comparatif avec la branche master.*
