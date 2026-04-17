---
description: Procédure pour analyser les changements de la branche courante par rapport à master et mettre à jour la documentation d'architecture centrale.
---

### 📝 Workflow : Mise à jour de la Documentation (GBuffer)

**1. Génération du Diff (Comparaison avec Master)**
* S'assurer que la branche courante est propre et à jour.
* **Action IA :** Demander à l'agent de comparer la branche courante avec `master` (via l'IDE ou en lui fournissant un `git diff master...HEAD`). S'il y a trop de fichiers, lui demander de cibler spécifiquement les modifications touchant au système de rendu et au GBuffer via `clion-native`.

**2. Analyse et Extraction du Contexte (RAG)**
* **Action IA :** Demander à l'agent : *"Lis le fichier actuel `docs/GBuffer Architecture.md`, puis analyse les différences C++ de notre branche pour identifier quelles parties de cette documentation sont devenues obsolètes."*
* **Action IA :** Utiliser `search_all_documents` pour retrouver d'éventuelles décisions de conception récentes enregistrées dans la mémoire mais non encore formalisées dans le document principal.

**3. Rédaction de la Mise à Jour**
* **Action IA :** Faire rédiger par l'agent les nouvelles sections ou les corrections du fichier `docs/GBuffer Architecture.md`. Lui demander d'utiliser un formatage rigoureux (mise à jour des structures de données, blocs de code C++ actualisés).
* Revoir manuellement les ajouts proposés par l'IA et valider les choix architecturaux.

**4. Validation et Ingestion (RAG)**
* Sauvegarder la nouvelle version du fichier `docs/GBuffer Architecture.md`.
* **Action IA :** Exécuter immédiatement `process_uploads` (ou `add_document` selon la configuration) pour forcer la mise à jour de la base vectorielle. Il est impératif que l'IA remplace ses anciennes connaissances par cette nouvelle vérité architecturale.