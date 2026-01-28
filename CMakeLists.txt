cmake_minimum_required(VERSION 3.16)
project("Project_SIMILI")
add_definitions(-DUNICODE -D_UNICODE)

set(CMAKE_CXX_STANDARD 17)

# Enable compiler cache if available (ccache/sccache)
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "Using ccache for compilation: ${CCACHE_PROGRAM}")
endif()

# Optimize build performance
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MP /O2")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MP /O2")


option(USE_CEF "Enable Chromium Embedded Framework for UI" ON) 

if(USE_CEF)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    foreach(flag_var
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO)
        if(${flag_var} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
        endif()
    endforeach()
endif()


#please download the SDL3-devel-3.2.14-VC.zip from its official github page and create on your C drive the following folder configuration:
#set SDl3
set(SDL3_INCLUDE_DIR "C:/libs/SDL3/include")
set(SDL3_LIBRARY "C:/libs/SDL3/lib/x64/SDL3.lib")

if(NOT EXISTS ${SDL3_LIBRARY})
    message(FATAL_ERROR "SDL3 library not found at ${SDL3_LIBRARY}")
endif()


#then save save this file to create the build folder.
#This project has been made and tested with VS code. So please use CTRL + Shift + P and choose : C/C++: Select Configuration
#and then choose : MSVC configuration
#Use CTRL + Shift + B to build the project thanks to file .vscode/tasks.json
#The C_Cpp_properties.json file, indeed, configure the intellisense for this project and the path to the compiler.

# Include dirs
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/ThreeDInteractions
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/ThreeDModes
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/MeshEdit
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/PrimitivesCreation
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/ProjectBuilder
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/SaveLoadSystem
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/Obj_Export_Import
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Layout_Theme
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/images
    ${CMAKE_CURRENT_SOURCE_DIR}/src/InternalLogic
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WorldObjects
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WorldObjects/Basic
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WorldObjects/Camera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WorldObjects/Entities
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WorldObjects/Mesh_DNA
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WorldObjects/DebugRay
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Resources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Services
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Services/middleware
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/example
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/glad
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/glm
    ${SDL3_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty
)

# Source files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)

# Exclude LEGACY folders and _LEGACY files explicitly
set(EXCLUDE_DIRS "LEGACY" "legacy" "UnitTest" "unitTest" "test" "tests" "Test")
foreach(TMP_PATH ${SOURCES})
    foreach(EXCLUDE_DIR ${EXCLUDE_DIRS})
        string(FIND ${TMP_PATH} "/${EXCLUDE_DIR}/" EXCLUDE_DIR_FOUND)
        if(NOT ${EXCLUDE_DIR_FOUND} EQUAL -1)
            list(REMOVE_ITEM SOURCES ${TMP_PATH})
            break()
        endif()
    endforeach()
    # Also exclude _LEGACY suffix files
    string(FIND ${TMP_PATH} "_LEGACY.cpp" LEGACY_SUFFIX_FOUND)
    if(NOT ${LEGACY_SUFFIX_FOUND} EQUAL -1)
        list(REMOVE_ITEM SOURCES ${TMP_PATH})
    endif()
endforeach()

list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/ImGuizmo/vcpkg-example/main.cpp")
list(FILTER SOURCES EXCLUDE REGEX "src/ThirdParty/imgui/backends/.*\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "src/ThirdParty/imgui/examples/.*\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "src/ThirdParty/imgui/misc/.*\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "src/ThirdParty/glm/.*\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "src/SIMILI_Frontend/.*\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "src/SIMILI_Services/middleware/.*\\.cpp$")

# Legacy UI code excluded from compilation (see EXCLUDE REGEX above)
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/ui_process_manager.cpp")

# Add CEF sources from SIMILI_Frontend (exclude main_ui.cpp - we use main.cpp now)
file(GLOB CEF_UI_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic/CameraControl/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic/HTMLTextureRenderer/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic/Keymanagement/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic/Raycasting/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic/ClickHandling/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic/ContextualMenuLogic/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic/FrameDatas/*.cpp"
)
list(REMOVE_ITEM CEF_UI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/main_ui.cpp")
list(APPEND SOURCES ${CEF_UI_SOURCES})

# Add SIMILI_Services/middleware sources explicitly
list(APPEND SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Services/middleware/SimpleHttpServer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Services/middleware/ConsoleLogger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Services/middleware/HttpSession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Services/middleware/HttpListener.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Services/middleware/ContextRegistry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Services/router/RouterSim.cpp
)

# Add KeyManager from SIMILI_Frontend (needed for ECS input system)
list(APPEND SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic/Keymanagement/KeyManager.cpp    "${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic/Keymanagement/MouseStates/Mouse_State.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic/Keymanagement/MouseStates/Mouse_Above_Overlay_State.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic/Keymanagement/MouseStates/Mouse_Outside_Overlay_State.cpp")


file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS src/*.hpp)

# ImGui sources (including Win32 backend for OverlayViewport)
file(GLOB IMGUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/backends/imgui_impl_opengl3.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/backends/imgui_impl_win32.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/imgui_demo.cpp
)

add_library(imgui STATIC
    ${IMGUI_SOURCES}
)



target_include_directories(imgui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/backends
)

target_compile_definitions(imgui PRIVATE IMGUI_USER_CONFIG="my_imgui_config.h")


# ImGuizmo sources
file(GLOB IMGUIZMO_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/ImGuizmo/ImGuizmo.cpp
)
list(APPEND SOURCES ${IMGUIZMO_SOURCES})

# Glad sources
file(GLOB_RECURSE GLAD_SOURCES CONFIGURE_DEPENDS src/ThirdParty/glad/*.c)

# Exclude GLAD from Unity builds (C files, not C++)
set_source_files_properties(${GLAD_SOURCES} PROPERTIES
    SKIP_PRECOMPILE_HEADERS ON
    SKIP_UNITY_BUILD_INCLUSION ON
)

# GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "GLFW: Build the examples" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "GLFW: Build the tests" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "GLFW: Build the docs" FORCE)
set(GLFW_INSTALL OFF CACHE INTERNAL "GLFW: Disable install" FORCE)
add_subdirectory(src/ThirdParty/GLFW)

# CEF (Chromium Embedded Framework)
if(USE_CEF)
    add_subdirectory(src/ThirdParty/CEF)
endif()

# tinyfiledialogs
add_library(tinyfiledialogs STATIC
    src/ThirdParty/tinyfiledialogs/tinyfiledialogs.c
)

target_include_directories(tinyfiledialogs PUBLIC
    src/ThirdParty/tinyfiledialogs
)


# OpenGL
find_package(OpenGL REQUIRED)
find_library(GLU_LIBRARY NAMES GLU glu32)

# Boost libraries - Static linking for HTTP server
set(Boost_USE_STATIC_LIBS ON)          # Link statically
set(Boost_USE_MULTITHREADED ON)        # Use multithreaded version
if(USE_CEF)
    set(Boost_USE_STATIC_RUNTIME ON)   # Static runtime (matches CEF's /MT)
endif()
find_package(Boost REQUIRED COMPONENTS system)
if(NOT Boost_FOUND)
    message(FATAL_ERROR "Boost not found. Please install via vcpkg: vcpkg install boost-beast:x64-windows-static")
endif()
message(STATUS "Boost found: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost libraries: ${Boost_LIBRARIES}")

# OpenSSL - Required for HTTPS support
find_package(OpenSSL REQUIRED)
if(NOT OPENSSL_FOUND)
    message(FATAL_ERROR "OpenSSL not found. Please install via vcpkg: vcpkg install openssl:x64-windows-static")
endif()
message(STATUS "OpenSSL found: ${OPENSSL_INCLUDE_DIR}")
message(STATUS "OpenSSL libraries: ${OPENSSL_LIBRARIES}")

# Executable (Windows subsystem for wWinMain)
if(WIN32)
    add_executable(${PROJECT_NAME} WIN32
        ${SOURCES}
        ${HEADERS}
        ${GLAD_SOURCES}
    )
else()
    add_executable(${PROJECT_NAME}
        ${SOURCES}
        ${HEADERS}
        ${GLAD_SOURCES}
    )
endif()

# Unity builds for faster compilation (optional - can be disabled if causing issues)
set_target_properties(${PROJECT_NAME} PROPERTIES
    UNITY_BUILD ON
    UNITY_BUILD_BATCH_SIZE 16
)

# Precompiled Headers (PCH) for faster compilation
# Only for C++ files, exclude C files like GLAD
target_precompile_headers(${PROJECT_NAME} PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/src/pch.hpp>"
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/ThreeDInteractions
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/ThreeDSceneDNA
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/SaveLoadSystem
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/MeshEdit
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/Obj_Export_Import
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/PrimitivesCreation
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/ProjectBuilder
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Layout_Theme
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/images
    ${CMAKE_CURRENT_SOURCE_DIR}/src/InternalLogic
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WorldObjects
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WorldObjects/Basic
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WorldObjects/Camera
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WorldObjects/Entities
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WorldObjects/Mesh_DNA
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Resources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/example
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/GLFW/include  
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/ImGuizmo
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/nlohmann
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Services
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Services/router
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Services/types
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Services/middleware
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic/CameraControl
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic/HTMLTextureRenderer
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic/Keymanagement
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic/Raycasting
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic/ClickHandling
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic/ContextualMenuLogic
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SIMILI_Frontend/viewportLogic/FrameDatas
    ${SDL3_INCLUDE_DIR}
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/tinyfiledialogs
)

# Add CEF include directories if enabled
if(USE_CEF)
    target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/CEF/cef_binary
        ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/CEF/cef_binary/include
    )
endif()

target_include_directories(imgui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/GLFW/include
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    OpenGL::GL
    ${GLU_LIBRARY}
    glfw
    imgui
    tinyfiledialogs
    ${SDL3_LIBRARY}
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    ws2_32
    crypt32
)

# Link CEF if enabled
if(USE_CEF)
    target_link_libraries(${PROJECT_NAME} PRIVATE CEF_Interface)
    target_compile_definitions(${PROJECT_NAME} PRIVATE USE_CEF)
    
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        "${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/CEF/cef_binary/Release/libcef.lib"
    )
endif()