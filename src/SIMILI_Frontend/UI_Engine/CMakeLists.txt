cmake_minimum_required(VERSION 3.16)
project(SIMILI_UI)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CEF_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/CEF/cef_binary")
if(NOT EXISTS "${CEF_ROOT}")
	message(FATAL_ERROR "CEF not found at ${CEF_ROOT}")
endif()

add_subdirectory(${CEF_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/cef_binary)

# GLAD library
set(GLAD_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/glad")
add_library(glad STATIC
	${GLAD_DIR}/glad.c
)
target_include_directories(glad PUBLIC ${GLAD_DIR}/include)

# GLM (header-only library)
set(GLM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/glm")

# Boost libraries - Static linking for standalone distribution
set(Boost_USE_STATIC_LIBS ON)          # Link statically
set(Boost_USE_MULTITHREADED ON)        # Use multithreaded version
set(Boost_USE_STATIC_RUNTIME ON)       # Static runtime (matches CEF's /MT)
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

# Force CMake to detect new files
file(GLOB SIMILI_UI_SOURCES 
	"${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)
file(GLOB SIMILI_UI_HEADERS 
	"${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

# Add SIMILI Services (Router) sources - specific files only, not examples
set(SIMILI_ROUTER_SOURCES 
	"${CMAKE_CURRENT_SOURCE_DIR}/../../SIMILI_Services/router/RouterSim.cpp"
)
set(SIMILI_ROUTER_HEADERS 
	"${CMAKE_CURRENT_SOURCE_DIR}/../../SIMILI_Services/router/RouterSim.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../SIMILI_Services/types/RouterTypes.hpp"
)

# Add SIMILI Server sources
set(SIMILI_SERVER_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/../../SIMILI_Services/middleware/SimpleHttpServer.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../SIMILI_Services/middleware/ConsoleLogger.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../SIMILI_Services/middleware/HttpSession.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../SIMILI_Services/middleware/HttpListener.cpp"
)
set(SIMILI_SERVER_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/../../SIMILI_Services/middleware/SimpleHttpServer.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../SIMILI_Services/middleware/ConsoleLogger.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../SIMILI_Services/middleware/HttpSession.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../SIMILI_Services/middleware/HttpListener.hpp"
)

message(STATUS "SIMILI_UI sources: ${SIMILI_UI_SOURCES}")
message(STATUS "SIMILI_ROUTER sources: ${SIMILI_ROUTER_SOURCES}")

add_executable(SIMILI_UI WIN32
	${SIMILI_UI_SOURCES}
	${SIMILI_UI_HEADERS}
	${SIMILI_ROUTER_SOURCES}
	${SIMILI_ROUTER_HEADERS}
	${SIMILI_SERVER_SOURCES}
	${SIMILI_SERVER_HEADERS}
)

target_link_libraries(SIMILI_UI PRIVATE
	libcef_dll_wrapper
	${CEF_STANDARD_LIBS}
	${CEF_ROOT}/Release/libcef.lib
	glad
	opengl32
	Boost::system
	OpenSSL::SSL
	OpenSSL::Crypto
	ws2_32
	crypt32
)

target_include_directories(SIMILI_UI PRIVATE
	${CEF_ROOT}
	${CEF_ROOT}/include
	${GLAD_DIR}/include
	${GLM_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../../SIMILI_Services
	${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty
	${Boost_INCLUDE_DIRS}
	${OPENSSL_INCLUDE_DIR}
)

target_compile_definitions(SIMILI_UI PRIVATE
	UNICODE
	_UNICODE
	WIN32_LEAN_AND_MEAN
	NOMINMAX
	_WIN32_WINNT=0x0601
)


set(CEF_BINARY_DIR "${CEF_ROOT}/Release")
set(CEF_RESOURCE_DIR "${CEF_ROOT}/Resources")

install(TARGETS SIMILI_UI DESTINATION bin)
install(DIRECTORY ${CEF_BINARY_DIR}/ DESTINATION bin FILES_MATCHING PATTERN "*.dll")
install(DIRECTORY ${CEF_RESOURCE_DIR}/ DESTINATION bin)
