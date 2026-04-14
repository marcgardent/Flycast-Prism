### État de l'implémentation du G-Buffer et comparaison avec Master

Ce rapport détaille les modifications techniques apportées par la branche `gbuffer` par rapport à `master`, basées sur l'analyse du code source et des fichiers de configuration.

#### 1. Nouvelles Fonctionnalités et Architecture

L'implémentation introduit un nouveau mode de rendu différé (G-Buffer) pour Vulkan, permettant l'extraction de données de scène (Albedo, Normales, Profondeur) vers des tampons séparés.

- **Nouveau Type de Rendu** : Ajout de `RenderType::Vulkan_GBuffer` (valeur `7`) dans `core/types.h`.
- **Moteur G-Buffer Dédié** : Création de `core/rend/vulkan/gbuffer/gbuffer_renderer.cpp`. Ce fichier implémente `GBufferVulkanRenderer`, qui gère deux attachements de couleur :
  - **Albedo** : Format `eR8G8B8A8Unorm` (Location 0).
  - **Normales** : Format `eR16G16B16A16Sfloat` (Location 1) pour une précision accrue.
- **Gestion du Pipeline** : Mise à jour de `core/rend/vulkan/pipeline.cpp` pour supporter plusieurs attachements de mélange (`ColorBlendAttachmentState`) lorsque le mode G-Buffer est actif.

#### 2. Évolution des Shaders (GLSL)

Les shaders Vulkan dans `core/rend/vulkan/shaders.cpp` ont été profondément remaniés :
- **Entrées/Sorties** :
  - Ajout d'une entrée de normale au vertex shader (`layout (location = 4) in vec3 in_normal`).
  - Support de deux sorties dans le fragment shader si `GBUFFER == 1` (`FragColor` et `NormalColor`).
- **Calcul des Normales** :
  - Utilisation des normales de sommets (`vtx_normal`) si disponibles.
  - Calcul de secours via le produit vectoriel des dérivées partielles (`normalize(cross(dFdx(vtx_pos), dFdy(vtx_pos)))`) pour les géométries sans normales explicites.
- **Profondeur Logarithmique** : Calcul et affichage de `gl_FragDepth` via `log2(1.0 + max(w, -0.999999)) / 34.0`, permettant une visualisation précise du tampon de profondeur.
- **Support Translucide** : Logique spécifique pour afficher la profondeur/normales des objets translucides (fumée, poussière) en utilisant un seuil d'alpha (discard si `< 0.2`).

#### 3. Interface et Débogage

- **Configuration** : Ajout de nouvelles options dans `core/cfg/option.h` :
  - `ShowDepth` : Affiche le tampon de profondeur.
  - `ShowNormals` : Affiche le tampon des normales.
  - `ShowDepthOpaqueOnly` : Filtre les objets translucides lors de l'affichage de la profondeur.
- **Interface Utilisateur** : Ajout de l'option "G-Buffer" dans les paramètres vidéo Vulkan (`core/ui/settings_video.cpp`), marquée comme "Experimental".
- **Raccourcis Clavier (SDL)** : Implémentation dans `core/sdl/sdl.cpp` de bascules en temps réel :
  - `Alt+1` : Mode Profondeur.
  - `Alt+2` : Mode Normales.
  - `Alt+0` : Retour au rendu Albedo standard.

#### 4. Modifications de Structure et Nettoyage

- **Drawer** : Dans `core/rend/vulkan/drawer.cpp`, le rendu des volumes d'ombre (`ModVol`) est désactivé lors de l'affichage des normales pour éviter les artefacts visuels sur le G-Buffer.
- **Système de Vertex** : Mise à jour de `core/hw/pvr/ta_vtx.cpp` pour assurer la transmission correcte des coordonnées `Z` et `W` nécessaires aux calculs de position dans les shaders.
- **Fichiers supprimés** : Suppression de `FEASIBILITY.md` et d'autres fichiers temporaires de recherche au profit de cette implémentation concrète.

#### 5. Screen Space Ambient Occlusion (SSAO)
Un algorithme de SSAO personnalisé a été implémenté directement dans le fragment shader du G-Buffer.
- **Méthode** : Échantillonnage pseudo-aléatoire (8 samples) autour du fragment.
- **Calcul** : Utilise les dérivées de position (`dFdx`, `dFdy`) pour simuler le plan tangent et estimer l'occlusion locale par la courbure géométrique.
- **Visualisation** : 
    - `Alt+1` : Depth Map (Z-Buffer).
    - `Alt+2` : Normal Map.
    - `Alt+3` : SSAO Pur (Noir & Blanc).
    - `Alt+0` : Rendu final (Albedo * AO).

#### 6. État d'Intégration et Stabilité
La branche `gbuffer` est désormais stable pour le rendu différé avec AO intégrée.
- **Inclusion** : Support complet des normales, de la profondeur logarithmique et du SSAO expérimental.
- **Persistance** : L'option SSAO est désormais sauvegardée globalement dans la configuration.
- **Correction** : Nettoyage du code source des shaders pour corriger des erreurs de compilation (caractères spéciaux).
- **Exclusion** : Les techniques d'Ambient Occlusion (CACAO) restent sur la branche `gbuffer-cacao`.

#### 7. Refactorisation et Maintenance
- **Externalisation** : Le code GLSL a été déplacé dans `core/rend/vulkan/shaders/` pour améliorer la lisibilité et faciliter l'édition avec coloration syntaxique.
- **Inclusion** : Utilisation des littéraux de chaîne brute C++ (`R"(...)"`) dans les fichiers `.vert`/`.frag`/`.glsl` pour permettre l'inclusion directe par le préprocesseur via `#include`.
- **Réglages SSAO** : Les paramètres de l'algorithme SSAO (samples, radius, strength, etc.) sont désormais regroupés en début de fichier `vulkan_main.frag`.

