---
description: Approche sécurisée pour modifier et moderniser l'architecture existante avec l'assistance des outils MCP, garantissant zéro régression.
---

### ♻️ Workflow : Refactoring (Zéro Régression)

**1. État des Lieux et Validation Initiale**
Avant de modifier le code existant, s'assurer que le comportement actuel est stable.
* Demander à l'utilisateur de lancer le logiciel et de confirmer manuellement que la zone à refactoriser fonctionne parfaitement en l'état.
* **Action IA :** Utiliser `search_all_documents` pour lister toutes les dépendances conceptuelles et notes historiques liées au module que l'on s'apprête à modifier.

**2. Élaboration du Plan de Refactoring**
* **Action IA :** Demander à l'agent de proposer une nouvelle structure étape par étape en utilisant impérativement `sequential-thinking`. L'objectif est d'améliorer les performances, la modularité ou la lisibilité *sans altérer le comportement final*.

**3. Exécution Chirurgicale (IDE)**
* **Action IA :** S'appuyer massivement sur `clion-native` pour le renommage de masse, l'extraction de méthodes ou le déplacement de classes. Cela garantit que les symboles C++ sont correctement mis à jour dans tout le projet Flycast.

**4. Validation Différentielle (Manuelle)**
* Recompiler l'intégralité du projet.
* Demander à l'utilisateur d'effectuer un test manuel rigoureux du logiciel pour s'assurer qu'aucune régression n'a été introduite.
* **Action IA :** En cas de plantage post-refactoring, exiger la stack trace et utiliser l'IDE pour corriger immédiatement la régression avant de poursuivre.

**5. Mise à jour des Conventions d'Architecture**
* **Action IA :** Si le refactoring introduit un nouveau Design Pattern fondamental ou une nouvelle convention de codage, utiliser l'outil `add_document` pour l'ajouter aux règles de la base de connaissances vectorielle.