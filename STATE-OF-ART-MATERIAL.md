# Architecture interne de Flycast : Données Matériau

## Les structures clés à connaître
Dans le codebase, la donnée "matériau" native est portée par trois structs par polygone, définies dans `core/hw/pvr/ta_ctx.h` et `core/hw/pvr/pvr_mem.h` :

* **TSP** — Texture/Shading Processor instruction word.
* **TCW** — Texture Control Word.
* **PCW** — Parameter Control Word (contient `list_type`).
* **ISP** — Image Synthesis Processor instruction word.

Le champ `PCW.list_type` (3 bits) définit la passe matériau native :

```cpp
enum ListType {
    ListType_Opaque           = 0,  // Géométrie solide
    ListType_Opaque_Modifier  = 1,  // Volumes shadow sur opaque
    ListType_Translucent      = 2,  // Alpha blending
    ListType_Translucent_Modifier = 3, // Volumes shadow sur translucide
    ListType_Punch_Through    = 4,  // Alpha clip binaire (masque)
};
```

---

## Où hooker dans le pipeline de rendu
Le pipeline OpenGL de Flycast suit ce chemin dans `core/rend/gles/` :

1. **ta_ctx** : Display lists côté CPU.
2. **RenderFrame()** : Boucle principale de rendu.
3. **DrawList(ListType)** : Itération sur les passes.
   * `SetupMaterial(poly)` : Lecture des TSP/TCW.
   * `glDraw*()` : Appel de dessin.

Le fichier central est **`core/rend/gles/gles.cpp`**. C'est là que tu as accès simultanément à PCW, TSP, et TCW juste avant le draw call.

---

## Stratégie d'implémentation du G-Buffer Material

### Étape 1 — Encodage du Material ID (8 bits)
Proposition d'encodage optimisé pour le post-process :

| Bits | Description | Source |
| :--- | :--- | :--- |
| **7-5** | list_type (0-4) | PCW.list_type |
| **4** | has_texture | TCW.TexEnable |
| **3** | is_gouraud | TSP.ShadInstr |
| **2** | has_bump | TCW.PixelFmt |
| **1** | fog_enabled | TSP.FogControl |
| **0** | palette | TCW.PixelFmt |

### Étape 2 — Ajout du Render Target dans le G-Buffer
Dans ta structure G-Buffer côté C++, on ajoute une texture `GL_R8UI` :

```cpp
// Initialisation du FBO G-Buffer
glGenTextures(1, &materialTex);
glBindTexture(GL_TEXTURE_2D, materialTex);
glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, width, height, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, nullptr);

// Attachement au FBO (Attachment 3 par exemple)
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, materialTex, 0);
```

---

## Étape 5 — Visualisation et Débogage (Mapping de couleur)

Comme les IDs sont des entiers très petits, le buffer apparaîtra noir. Il faut créer une passe de visualisation qui transforme l'ID en couleur contrastée.

### Shader de Visualisation (Post-process)
Ce shader utilise un hash pour générer une couleur unique à partir de l'ID stocké.

```glsl
// Fragment Shader de Debug
uniform usampler2D u_MaterialBuffer;
out vec4 fragColor;

vec3 hashColor(uint id) {
   // Fonction de hash simple pour transformer l'ID en couleur RGB
   uint h = id * 2654435761u;
   return vec3(float((h >> 16) & 255u) / 255.0,
   float((h >> 8) & 255u) / 255.0,
   float(h & 255u) / 255.0);
}

void main() {
   uint matID = texture(u_MaterialBuffer, TexCoord).r;

   if (matID == 0u) {
      fragColor = vec4(0.1, 0.1, 0.1, 1.0); // Gris sombre pour le vide
   } else {
      fragColor = vec4(hashColor(matID), 1.0);
   }
}
```

---

## Points d'attention
* **Modifier Volumes** : Les ombres portées (`Modifier_Modifier`) ont leur propre path (`DrawModVols`). Il faut leur assigner un ID distinct (ex: `0xE0`).
* **Render-to-Texture (RTT)** : Certaines passes de post-process internes au jeu ont leur propre FBO. Assure-toi que ton attachement Material est présent si tu veux capturer ces effets.
* **Extraction EXR** : Lors de l'export final, le canal Material doit être inclus comme un canal entier ou float normalisé (`matID / 255.0`).