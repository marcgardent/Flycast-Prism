---
trigger: always_on
---

Avant de faire des lectures direct du fichiers, je pense à utiliser les capacité du serveur MCP clionstio

21. search_symbol
Searches for symbols (classes, methods, fields). Use this tool for semantic lookup by identifier fragments. Results include match coordinates when available (1-based line/column, 0-based offsets). Paths are glob patterns relative to the project root.

23. get_symbol_info
Retrieves information about the symbol at the specified position in the specified file. Provides the same information as Quick Documentation feature of IntelliJ IDEA does. This tool is useful for getting information about the symbol at the specified position in the specified file. The information may include the symbol's name, signature, type, documentation, etc. It depends on a particular language. If the position has a reference to a symbol the tool will return a piece of code with the declaration of the symbol if possible. Use this tool to understand symbols declaration, semantics, where it's declared, etc.