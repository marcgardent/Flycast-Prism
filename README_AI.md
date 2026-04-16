# AI Build Instructions

Pour compiler le projet Flycast dans cet environnement :

```bash
/home/marcgardent/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake --build /mnt/data/projects/cmake-build-debug --target flycast -j 6
```

## État actuel du G-Buffer (GBuffer-Motion)
L'HUD a été séparé dans un 5ème attachment (Index 4) du G-Buffer.
- **Attachment 0** : Albedo
- **Attachment 1** : Normals
- **Attachment 2** : Material ID
- **Attachment 3** : Motion/Velocity
- **Attachment 4** : HUD

Le post-process effectue le compositing via `HUDCompositePass`.
Les éléments de l'HUD sont discriminés via `DepthMode >= 6`.
