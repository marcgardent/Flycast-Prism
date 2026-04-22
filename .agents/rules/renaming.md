---
trigger: manual
---

j'utilise le serveur MCP clionstio pour faire des renomages


24. rename_refactoring
Renames a symbol (variable, function, class, etc.) in the specified file. Use this tool to perform rename refactoring operations. The `rename_refactoring` tool is a powerful, context-aware utility. Unlike a simple text search-and-replace, it understands the code's structure and will intelligently update ALL references to the specified symbol throughout the project, ensuring code integrity and preventing broken references. It is ALWAYS the preferred method for renaming programmatic symbols. Requires three parameters: - pathInProject: The relative path to the file from the project's root directory (e.g., `src/api/controllers/userController.js`) - symbolName: The exact, case-sensitive name of the existing symbol to be renamed (e.g., `getUserData`) - newName: The new, case-sensitive name for the symbol (e.g., `fetchUserData`). Returns a success message if the rename operation was successful. Returns an error message if the file or symbol cannot be found or the rename operation failed.