---
description: Brainstorming
---

---
description: Phase de réflexion stratégique et technique sans modification de code, visant à explorer des solutions ou des architectures futures.
---

### 🧠 Workflow : Brainstorming & Conception Pure

**1. Cadrage de la Réflexion**
* Définir clairement le sujet de réflexion (ex: "Optimisation du Tile-Based Rendering" ou "Refonte de la gestion des descripteurs Vulkan").
* **Action IA :** Préciser qu'il est INTERDIT de modifier le code durant cette phase. L'objectif est l'exploration pure.

**2. Exploration de l'Existant et des Contraintes**
* **Action IA :** Utiliser `search_all_documents` pour lister toutes les décisions passées, les échecs documentés et les contraintes matérielles (RTX 5070) liées au sujet.
* **Action IA :** Interroger `vulkan-docs-local` pour identifier les fonctionnalités de l'API 1.3 qui pourraient simplifier ou révolutionner l'approche actuelle.

**3. Analyse Profonde et Pensée Divergente**
* **Action IA :** Utiliser `sequential-thinking` pour peser le pour et le contre de plusieurs approches (A, B, C). L'agent doit simuler les goulots d'étranglement potentiels et les impacts sur le reste du moteur.
* Demander à l'agent de proposer des solutions innovantes, même si elles s'éloignent de l'implémentation actuelle.

**4. Synthèse et Validation Théorique**
* Résumer les meilleures idées retenues lors du brainstorming.
* **Action IA :** Demander à l'agent de rédiger une "Note d'Intention Technique" résumant l'approche choisie.

**5. Archivage du Savoir (RAG)**
* **Action IA :** Enregistrer cette réflexion dans la base de connaissances via `add_document` avec le tag `[Brainstorm]`. Cela permettra aux futurs workflows de "Feature" ou de "Refactoring" de se baser sur cette réflexion préalable.