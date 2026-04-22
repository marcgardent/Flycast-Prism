# Brief : Implémentation des Metadata Buffers (TextureHash & PolyData)

L'objectif est de capturer les métadonnées de chaque pixel pour faciliter l'identification des objets dans le **PolyExclusion Engine** et permettre un export EXR riche.

---

### 1. Structure des Buffers techniques
Pour ne pas gaspiller de bande passante en temps normal, ces buffers sont des **Render Targets** supplémentaires attachées au G-Buffer uniquement lors de l'activation du mode "Debug" ou "Export".

* **Buffer A : TextureHash (R32_UINT)**
    * Stocke le `u32 hash_val` de la texture utilisée par le fragment.
* **Buffer B : PolyData (RGBA32_FLOAT)**
    * **R** : Position X du centroïde (ou premier sommet).
    * **G** : Position Y.
    * **B** : Position Z.
    * **A** : PolyCount (casté en float).

---

### 2. Gestion Dynamique de la Mémoire (Mode Export EXR)
Il est inutile de réserver de la VRAM pour ces textures 4K/8K en permanence.

* **Mode Standard** : Les attachments ne sont pas créés. Le `VkRenderPass` n'inclut que l'Albedo, les Normales et le Depth.
* **Mode Export EXR (On-Demand)** :
    1.  L'utilisateur déclenche la capture.
    2.  Le moteur détruit temporairement la Framebuffer actuelle.
    3.  On recrée le G-Buffer avec les 2 attachments supplémentaires.
    4.  On effectue un rendu de frame.
    5.  On récupère les données via `vkCmdCopyImageToBuffer`.
    6.  On repasse en mode standard pour libérer la VRAM.

---

### 3. Logique Shader (G-Buffer Fragment)

L'écriture dans ces buffers se fait via des `out` indexés qui ne sont actifs que si le pipeline le permet.

```glsl
// Uniquement dans le pipeline de capture
layout(location = 3) out uint outTexHash;
layout(location = 4) out vec4 outPolyData;

void main() {
    // ... rendu normal ...
    
    if (capture_enabled) {
        outTexHash = pc.texHash;
        outPolyData = vec4(pc.polyPos.xyz, float(pc.polyCount));
    }
}
```

---

### 4. Utilisation pour le YAML
Grâce à l'export EXR, tu pourras ouvrir tes passes dans un outil de lecture de données (ou un simple script Python) :
1.  Tu cliques sur un pixel "buggé".
2.  Tu lis les valeurs exactes : `X: 124.5, Y: 0.1, Z: 512.2, Hash: 0x99887766, Count: 4`.
3.  Tu n'as plus qu'à copier-coller dans ton YAML.

---

### 5. Avantages
* **Zéro impact FPS** en jeu normal (les buffers n'existent pas).
* **Précision chirurgicale** : On identifie les objets au pixel près.
* **Compatibilité EXR** : Le format EXR supporte nativement les couches arbitraires en Float32, parfait pour stocker des coordonnées mondiales.