---
description: Méthodologie assistée par l'IA et la base vectorielle pour l'analyse, l'isolation et la résolution de bugs complexes (ex: crashs Vulkan).
---

### 🐛 Workflow : Débogage Avancé & Résolution

**1. Analyse Initiale du Problème**
* Reproduire le bug et demander à l'utilisateur de fournir la stack trace complète, les logs d'erreur ou de décrire le comportement anormal observé lors de l'exécution.
* **Action IA :** Analyser le log d'erreur et forcer l'outil `sequential-thinking` pour formuler des hypothèses sur la cause racine.

**2. Recherche de Précédents (RAG)**
* **Action IA :** Utiliser `search_all_documents` avec les mots-clés spécifiques de l'erreur (ex: *VK_ERROR_DEVICE_LOST*, *swapchain*) pour vérifier si ce crash a déjà été étudié, documenté ou partiellement résolu par le passé.

**3. Isolation et Inspection du Code**
* **Action IA :** Utiliser `clion-native` pour inspecter en profondeur les définitions des classes et fonctions impliquées. L'agent doit "lire" le flux d'exécution autour de la zone suspecte.

**4. Validation et Résolution**
* **Action IA :** Demander un correctif ciblé. Si le problème touche à l'API Vulkan, exiger de l'agent qu'il croise sa solution avec `vulkan-docs-local` pour s'assurer de la validité des spécifications de l'API.
* Appliquer le correctif et demander à l'utilisateur de compiler et lancer le logiciel pour tester si le crash/bug est bien résolu.

**5. Post-Mortem et Immunisation**
* **Action IA :** Utiliser `add_document` pour enregistrer un REX (Retour d'Expérience) avec un titre explicite (ex: *[Bug_Fix] Fuite mémoire Vulkan Texture*). Inclure le code fautif et la solution, afin que l'agent s'en souvienne pour les prochaines sessions.