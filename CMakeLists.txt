cmake_minimum_required(VERSION 3.16)
project("Project_SIMILI")
add_definitions(-DUNICODE -D_UNICODE)

set(CMAKE_CXX_STANDARD 17)


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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UI    
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UI/ThreeDModes
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UI/HierarchyInspectorLogic
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UI/ThreeDWindow
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/ThreeDInteractions
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/images
    ${CMAKE_CURRENT_SOURCE_DIR}/src/InternalLogic
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WorldObjects
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WorldObjects/Basic
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Resources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/example
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/glad
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/glm

    ${SDL3_INCLUDE_DIR}
)

# Source files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/ImGuizmo/vcpkg-example/main.cpp")
list(FILTER SOURCES EXCLUDE REGEX "src/ThirdParty/imgui/backends/.*\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "src/ThirdParty/imgui/examples/.*\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "src/ThirdParty/imgui/misc/.*\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "src/ThirdParty/glm/.*\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "src/ThirdParty/glm/test/.*\\.cpp$")


file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS src/*.hpp)
file(GLOB_RECURSE UIS CONFIGURE_DEPENDS src/UI/*.ui)

# ImGui sources
file(GLOB IMGUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/backends/imgui_impl_glfw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/backends/imgui_impl_opengl3.cpp
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

# GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "GLFW: Build the examples" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "GLFW: Build the tests" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "GLFW: Build the docs" FORCE)
set(GLFW_INSTALL OFF CACHE INTERNAL "GLFW: Disable install" FORCE)
add_subdirectory(src/ThirdParty/GLFW)

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

#find DirectX 12


list(APPEND SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/DirectX12/DirectX12Renderer.cpp
)
list(APPEND HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/DirectX12/DirectX12Renderer.hpp
)

list(APPEND SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UI/DirectX12TestWindow.cpp
)
list(APPEND HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UI/DirectX12TestWindow.hpp
)

set(WINDOWS_SDK_VERSION "10.0.22000.0")
set(WINDOWS_SDK_INCLUDE_DIR "C:/Program Files (x86)/Windows Kits/10/Include/${WINDOWS_SDK_VERSION}")
set(WINDOWS_SDK_LIB_DIR "C:/Program Files (x86)/Windows Kits/10/Lib/${WINDOWS_SDK_VERSION}/um/x64")

include_directories(
    ${WINDOWS_SDK_INCLUDE_DIR}/um
    ${WINDOWS_SDK_INCLUDE_DIR}/shared
    ${WINDOWS_SDK_INCLUDE_DIR}/winrt
)

find_library(D3D12_LIBRARY d3d12 PATHS "${WINDOWS_SDK_LIB_DIR}")
find_library(DXGI_LIBRARY dxgi PATHS "${WINDOWS_SDK_LIB_DIR}")
find_library(D3DCOMPILER_LIBRARY d3dcompiler PATHS "${WINDOWS_SDK_LIB_DIR}")

if(NOT D3D12_LIBRARY OR NOT DXGI_LIBRARY OR NOT D3DCOMPILER_LIBRARY)
    message(FATAL_ERROR "DirectX 12 libraries not found")
endif()


# Executable
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${UIS}
    ${GLAD_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UI
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UI/ThreeDModes
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UI/HierarchyInspectorLogic
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UI/ThreeDWindow
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine/ThreeDInteractions
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/images
    ${CMAKE_CURRENT_SOURCE_DIR}/src/InternalLogic
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WorldObjects
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WorldObjects/Basic
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Resources
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/example
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/glad/include  
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/ImGuizmo
    ${SDL3_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/tinyfiledialogs
)

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
    ${D3D12_LIBRARY}
    ${DXGI_LIBRARY}
    ${D3DCOMPILER_LIBRARY}
    ${SDL3_LIBRARY}
)
