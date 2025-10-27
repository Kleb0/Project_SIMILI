# CMakeLists.txt for Chromium Embedded Framework (CEF)
cmake_minimum_required(VERSION 3.19)

project(CEF_Integration)

# Chemin vers le répertoire CEF
set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/cef_binary" CACHE PATH "Path to CEF binary distribution")

# Vérifier si CEF existe
if(NOT EXISTS "${CEF_ROOT}")
    message(FATAL_ERROR "CEF binary distribution not found at: ${CEF_ROOT}")
    message(FATAL_ERROR "Please download CEF from: https://cef-builds.spotifycdn.com/index.html")
    message(FATAL_ERROR "Extract to: ${CEF_ROOT}")
    return()
endif()

# Vérifier que CMakeLists.txt existe
if(NOT EXISTS "${CEF_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "CEF CMakeLists.txt not found. Please verify CEF extraction.")
    return()
endif()

message(STATUS "CEF Root found: ${CEF_ROOT}")

# Configuration CEF
set(USE_SANDBOX OFF CACHE BOOL "Enable CEF sandbox")

# Ajouter le projet CEF
add_subdirectory(${CEF_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/cef_binary)

# Créer une bibliothèque d'interface pour faciliter l'intégration
add_library(CEF_Interface INTERFACE)

target_include_directories(CEF_Interface INTERFACE
    ${CEF_ROOT}
)

target_link_libraries(CEF_Interface INTERFACE
    libcef_dll_wrapper
)

message(STATUS "CEF configuration complete")
message(STATUS "CEF Root: ${CEF_ROOT}")
