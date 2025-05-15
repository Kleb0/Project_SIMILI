cmake_minimum_required(VERSION 3.16)
project("Project_SIMILI")

set(CMAKE_CXX_STANDARD 17)

# Include dirs
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UI
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AbstractClass
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/images
    ${CMAKE_CURRENT_SOURCE_DIR}/src/InternalLogic
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WorldObjects
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/example
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/glad
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIR}
)

# Source files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(FILTER SOURCES EXCLUDE REGEX "src/ThirdParty/imgui/backends/.*\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "src/ThirdParty/imgui/examples/.*\\.cpp$")
list(FILTER SOURCES EXCLUDE REGEX "src/ThirdParty/imgui/misc/.*\\.cpp$")

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
)

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

# SDL2
find_path(SDL2_INCLUDE_DIR SDL.h PATH_SUFFIXES SDL2)
find_library(SDL2_LIBRARY NAMES SDL2)

if(NOT SDL2_INCLUDE_DIR OR NOT SDL2_LIBRARY)
    message(FATAL_ERROR "SDL2 not found")
endif()

find_path(SDL2_IMAGE_INCLUDE_DIR SDL_image.h PATH_SUFFIXES SDL2)
find_library(SDL2_IMAGE_LIBRARY NAMES SDL2_image)

# OpenGL
find_package(OpenGL REQUIRED)
find_library(GLU_LIBRARY NAMES GLU glu32)

# Executable
add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${UIS}
    ${IMGUI_SOURCES}
    ${GLAD_SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UI
    ${CMAKE_CURRENT_SOURCE_DIR}/src/Engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AbstractClass
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/images
    ${CMAKE_CURRENT_SOURCE_DIR}/src/InternalLogic
    ${CMAKE_CURRENT_SOURCE_DIR}/src/WorldObjects
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/imgui/example
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/glad/include  
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThirdParty/ImGuizmo
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${SDL2_LIBRARY}
    ${SDL2_IMAGE_LIBRARY}
    OpenGL::GL
    ${GLU_LIBRARY}
    glfw
)

set_target_properties(${PROJECT_NAME} PROPERTIES AUTOMOC_MACRO_NAMES "Q_OBJECT")
