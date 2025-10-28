# Chromium Embedded Framework (CEF) Integration

## Installation

1. **Download CEF**
   - Go to: https://cef-builds.spotifycdn.com/index.html
   - Choose Windows 64-bit version (Standard Distribution)
   - Recommended version: 120.x or higher

2. **Extract CEF**
   ```
   Extract contents to:
   e:\Project_SIMILI\Project\src\ThirdParty\CEF\cef_binary\
   ```

3. **Expected structure**
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

## CMake Configuration

To enable CEF in the main project, add to the root CMakeLists.txt:

```cmake
# Option to enable CEF
option(USE_CEF "Enable Chromium Embedded Framework" OFF)

if(USE_CEF)
    add_subdirectory(src/ThirdParty/CEF)
    target_link_libraries(Project_SIMILI PRIVATE CEF_Interface)
endif()
```

## Usage

Basic integration example:

```cpp
#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_render_handler.h"

// See examples in CEF documentation
```

## License

CEF is licensed under BSD. See: https://bitbucket.org/chromiumembedded/cef/src/master/LICENSE.txt

## Approximate Size

- Binary distribution: ~500 MB
- Runtime: ~200 MB
- No internet connection required for local operation

