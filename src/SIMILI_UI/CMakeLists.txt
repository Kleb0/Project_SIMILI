cmake_minimum_required(VERSION 3.16)
project(SIMILI_UI)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/CEF/cef_binary")
if(NOT EXISTS "${CEF_ROOT}")
    message(FATAL_ERROR "CEF not found at ${CEF_ROOT}")
endif()

add_subdirectory(${CEF_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/cef_binary)

# Collecter automatiquement tous les fichiers sources et headers
file(GLOB SIMILI_UI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
file(GLOB SIMILI_UI_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp")

add_executable(SIMILI_UI WIN32
    ${SIMILI_UI_SOURCES}
    ${SIMILI_UI_HEADERS}
)

target_link_libraries(SIMILI_UI PRIVATE
    libcef_dll_wrapper
    ${CEF_STANDARD_LIBS}
    ${CEF_ROOT}/Release/libcef.lib
)

target_include_directories(SIMILI_UI PRIVATE
    ${CEF_ROOT}
    ${CEF_ROOT}/include
)

target_compile_definitions(SIMILI_UI PRIVATE
    UNICODE
    _UNICODE
)


set(CEF_BINARY_DIR "${CEF_ROOT}/Release")
set(CEF_RESOURCE_DIR "${CEF_ROOT}/Resources")

install(TARGETS SIMILI_UI DESTINATION bin)
install(DIRECTORY ${CEF_BINARY_DIR}/ DESTINATION bin FILES_MATCHING PATTERN "*.dll")
install(DIRECTORY ${CEF_RESOURCE_DIR}/ DESTINATION bin)
