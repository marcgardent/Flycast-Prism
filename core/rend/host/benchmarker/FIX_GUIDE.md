# Guide de Correction : Z-Fighting & HUD Flickering

La reproduction via **GEO-10** montre qu'à des profondeurs extrêmes (Z proche de 1.0), la précision des `float32` n'est plus suffisante pour garantir la stabilité du tampon de profondeur, provoquant des clignotements sur le HUD.

## 1. Solution Immédiate : Le Depth Bias (Polygon Offset)

C'est la méthode la plus fiable pour forcer une couche (le HUD) à passer devant une autre. Comme l'API Flycast ne fournit pas de champ `bias`, vous devez l'appliquer dans votre implémentation du renderer.

### Option A : Dans le Pipeline (WebGPU/Vulkan/DX12)
Si vous utilisez un pipeline de rendu, vous pouvez activer le `depthBias`. Pour un HUD, un décalage constant est généralement suffisant.

```rust
// Exemple WGPU
depth_stencil: Some(wgpu::DepthStencilState {
    format: wgpu::TextureFormat::Depth32Float,
    depth_write_enabled: true,
    depth_compare: wgpu::CompareFunction::GreaterEqual, // Standard PVR
    bias: wgpu::DepthBiasState {
        constant: 2,           // Décale légèrement vers l'avant
        slope_scale: 1.0,      // Utile si le HUD est incliné
        clamp: 0.0,
    },
    stencil: wgpu::StencilState::default(),
})
```

### Option B : Dans le Vertex Shader (Solution Universelle)
Si vous ne voulez pas créer de pipelines spécifiques, vous pouvez ajouter un "nudge" (une petite impulsion) directement dans le Vertex Shader.

```wgsl
// rasterizer.wgsl
@vertex
fn vs_main(input: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    // ...
    var z_final = input.z_inv;
    
    // Si c'est un élément de HUD (proche de 1.0), on ajoute un epsilon
    if (z_final > 0.99) {
        z_final += 0.00001; 
    }
    
    out.position = vec4<f32>(input.x, input.y, z_final, 1.0);
    // ...
    return out;
}
```

## 2. Solution Structurelle : Reverse-Z

Le Z-buffer standard a moins de précision quand on s'éloigne de `0.0`. En utilisant un tampon en virgule flottante (`Depth32Float`) et en inversant la plage (Reverse-Z), on obtient une précision quasi-constante.

1.  Mappez `z_inv = 1.0` (proche) vers `0.0` dans le tampon.
2.  Utilisez la fonction de comparaison `LessEqual`.
3.  Cela donne beaucoup plus de bits de précision pour les valeurs proches de `1.0` (HUD).

## 3. Stabilité de l'OIT (Order Independent Transparency)

Si votre HUD est translucide (cas de **OIT-02**), le clignotement vient probablement de l'instabilité du tri.

- **Règle d'or** : Quand deux fragments ont la **même profondeur**, l'algorithme OIT doit respecter l'**ordre de soumission**.
- Dans une Linked-List (A-Buffer), assurez-vous que lors de la lecture de la liste, vous ne réordonnez pas les fragments de même profondeur de manière aléatoire (utilisez un tri stable).

## 4. Précision des Shaders

Assurez-vous d'utiliser `highp` (ou `f32` complet en WGSL) pour toutes les variables impliquées dans le calcul de la position.

```wgsl
// ÉVITEZ :
var pos = vec2<f16>(input.x, input.y); 

// UTILISEZ :
var pos = vec2<f32>(input.x, input.y);
```

---

> [!TIP]
> Pour vérifier la correction, relancez **GEO-10**. Si le rectangle Magenta ne clignote plus du tout pendant le "sweep", votre solution est robuste.
