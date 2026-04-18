---
description: Procédure standardisée pour le développement assisté par IA d'une nouvelle fonctionnalité, de l'idéation à l'intégration et la mise à jour du RAG.
---

### 🚀 Workflow : Nouvelle Feature

**1. Idéation et Vérification (La Phase de Découverte)**
Avant d'écrire la moindre ligne de code, s'assurer de ne pas casser l'architecture existante.
* **Action IA :** Demander : *"Je veux implémenter [Feature]. Utilise `search_all_documents` pour vérifier si nous avons déjà des notes d'architecture ou des contraintes connues concernant ce domaine."*
* **Action IA :** Vérifier les spécifications dans la documentation (`vulkan-docs-local`) pour valider la faisabilité technique.

**2. Planification Architecturale (Le Design)**
Définir comment la feature va s'intégrer dans le code C++.
* Créer la branche Git (`git checkout -b feature/nom`).
* **Action IA :** Déclencher `sequential-thinking` pour décomposer la logique architecturale étape par étape avant de générer le squelette du code.

**3. Implémentation (Le Code)**
La phase de programmation active au sein de l'IDE.
* **Action IA :** Utiliser `clion-native` pour naviguer, comprendre les dépendances, générer des fonctions ou refactoriser des portions existantes en direct.
* Préparer des instructions de test claires pour la validation manuelle de l'utilisateur.

**4. Tests et Débogage (Validation Manuelle)**
* **Action IA :** Compiler le projet via clion
* Demander à l'utilisateur de lancer le logiciel manuellement pour valider le comportement de la nouvelle feature.
* **Action IA :** En cas de plantage ou de comportement inattendu, demander explicitement la stack trace ou les logs d'erreur, puis tracer la source de l'erreur via l'IDE.

**5. Mise à jour de la Base de Connaissances (L'Héritage)**
L'étape cruciale pour alimenter le moteur RAG.
* Rédiger un fichier explicatif (`doc_feature.md`) détaillant le fonctionnement sous le capot et les choix techniques.
* **Action IA :** Exécuter `process_uploads` (ou `add_document`) pour ingérer ce document dans le cerveau vectoriel de l'agent.