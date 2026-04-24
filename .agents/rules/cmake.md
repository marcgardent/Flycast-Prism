---
trigger: always_on
---

le binaire cmake :
/home/marcgardent/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake 

la command de build 

/home/marcgardent/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake --build /mnt/data/projects/cmake-build-debug --target all -j 6

Pour compiler les tests

pour compiler le projet test :
/home/marcgardent/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake --build /mnt/data/projects/cmake-build-tests --target flycast -j 6