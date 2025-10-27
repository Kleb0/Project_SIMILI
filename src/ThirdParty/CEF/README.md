# Readme temporaire pour integration CEF
# Chromium Embedded Framework (CEF) Integration

## Installation

1. **Télécharger CEF**
   - Aller sur: https://cef-builds.spotifycdn.com/index.html
   - Choisir la version Windows 64-bit (Standard Distribution)
   - Version recommandée: 120.x ou supérieur

2. **Extraire CEF**
   ```
   Extraire le contenu dans:
   e:\Project_SIMILI\Project\src\ThirdParty\CEF\cef_binary\
   ```

3. **Structure attendue**
   ```
   CEF/
   ├── CMakeLists.txt
   ├── README.md
   └── cef_binary/
       ├── cmake/
       ├── include/
       ├── libcef_dll/
       ├── Release/
       │   ├── libcef.dll
       │   ├── chrome_elf.dll
       │   └── ...
       ├── Resources/
       │   ├── icudtl.dat
       │   ├── locales/
       │   └── ...
       └── CMakeLists.txt
   ```

## Configuration CMake

Pour activer CEF dans le projet principal, ajouter dans le CMakeLists.txt racine:

```cmake
# Option pour activer CEF
option(USE_CEF "Enable Chromium Embedded Framework" OFF)

if(USE_CEF)
    add_subdirectory(src/ThirdParty/CEF)
    target_link_libraries(Project_SIMILI PRIVATE CEF_Interface)
endif()
```

## Utilisation

Exemple d'intégration basique:

```cpp
#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_render_handler.h"

// Voir exemples dans la documentation CEF
```

## Licence

CEF est sous licence BSD. Voir: https://bitbucket.org/chromiumembedded/cef/src/master/LICENSE.txt

## Taille approximative

- Binary distribution: ~500 MB
- Runtime: ~200 MB
- Pas de connexion internet requise pour le fonctionnement local
