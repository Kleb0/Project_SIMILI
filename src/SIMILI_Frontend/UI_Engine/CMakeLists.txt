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

# GLFW
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "GLFW: Build the examples" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "GLFW: Build the tests" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "GLFW: Build the docs" FORCE)
set(GLFW_INSTALL OFF CACHE INTERNAL "GLFW: Disable install" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/GLFW ${CMAKE_CURRENT_BINARY_DIR}/glfw)

# ImGui library
file(GLOB IMGUI_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/imgui/imgui.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/imgui/imgui_draw.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/imgui/imgui_tables.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/imgui/imgui_widgets.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/imgui/backends/imgui_impl_glfw.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/imgui/backends/imgui_impl_opengl3.cpp"
)
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/imgui"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/imgui/backends"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/GLFW/include"
)
target_compile_definitions(imgui PRIVATE IMGUI_USER_CONFIG="my_imgui_config.h")

# ImGuizmo library
file(GLOB IMGUIZMO_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/ImGuizmo/ImGuizmo.cpp"
)

# Tinyfiledialogs library
set(TINYFILEDIALOGS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/tinyfiledialogs")
add_library(tinyfiledialogs STATIC
	${TINYFILEDIALOGS_DIR}/tinyfiledialogs.c
)
target_include_directories(tinyfiledialogs PUBLIC ${TINYFILEDIALOGS_DIR})

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

# Add Engine sources needed for 3D scene rendering
set(ENGINE_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/ThreeDScene.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/OpenGLContext.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/Guizmo.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/SimiliSelector.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/ThreeDObjectSelector.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/ThreeDInteractions/MeshTransform.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/ThreeDInteractions/VerticeTransform.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/ThreeDInteractions/FaceTransform.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/ThreeDInteractions/EdgeTransform.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/MeshEdit/ExtrudeFace.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/MeshEdit/EdgeLoop.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/MeshEdit/CutQuad.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/SaveLoadSystem/Save_Scene.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Camera/Camera.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Entities/ThreeDObject.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Entities/EmptyDummy.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Mesh/Mesh.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Mesh_DNA/Mesh_DNA.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Basic/Vertice.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Basic/Edge.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Basic/Face.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Basic/Quad.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Basic/Triangle.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Basic/Ngon.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/PrimitivesCreation/CreatePrimitive.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/ThreeDScene_DNA/ThreeDScene_DNA.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/GUIWindow.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/MainSoftwareGUI.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/HierarchyInspectorLogic/HierarchyInspector.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/HierarchyInspectorLogic/HandleHierarchyInteractions.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/HierarchyInspectorLogic/HierarchyListDrawing.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/ObjectInspectorLogic/ObjectInspector.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/ThreeDWindow/ThreeDWindow.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/ThreeDWindow/ClickHandler.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/ThreeDModes/Normal_Mode.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/ThreeDModes/Vertice_Mode.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/ThreeDModes/Face_Mode.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/ThreeDModes/Edge_Mode.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/ContextualMenu/ContextualMenu.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/EdgeLoopControl/EdgeLoopControl.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/UIdocking/Uidocking.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/OptionMenu/OptionsMenu.cpp"
	${IMGUIZMO_SOURCES}
)
set(ENGINE_HEADERS
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/ThreeDScene.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/OpenGLContext.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/Guizmo.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/SimiliSelector.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/ThreeDObjectSelector.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/ThreeDInteractions/MeshTransform.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/ThreeDInteractions/VerticeTransform.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/ThreeDInteractions/FaceTransform.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/ThreeDInteractions/EdgeTransform.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/MeshEdit/ExtrudeFace.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/MeshEdit/EdgeLoop.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/MeshEdit/CutQuad.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/SaveLoadSystem/Save_Scene.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Camera/Camera.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Entities/ThreeDObject.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Entities/EmptyDummy.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Mesh/Mesh.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Mesh_DNA/Mesh_DNA.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Basic/Vertice.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Basic/Edge.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Basic/Face.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Basic/Quad.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Basic/Triangle.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Basic/Ngon.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/PrimitivesCreation/CreatePrimitive.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/ThreeDScene_DNA/ThreeDScene_DNA.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/GUIWindow.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/MainSoftwareGUI.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/HierarchyInspectorLogic/HierarchyInspector.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/HierarchyInspectorLogic/HandleHierarchyInteractions.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/HierarchyInspectorLogic/HierarchyListDrawing.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/ObjectInspectorLogic/ObjectInspector.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/ThreeDWindow/ThreeDWindow.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/ThreeDWindow/ClickHandler.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/ThreeDModes/Normal_Mode.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/ThreeDModes/Vertice_Mode.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/ThreeDModes/Face_Mode.hpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../../UI/ThreeDModes/Edge_Mode.hpp"
)

message(STATUS "SIMILI_UI sources: ${SIMILI_UI_SOURCES}")
message(STATUS "SIMILI_ROUTER sources: ${SIMILI_ROUTER_SOURCES}")
message(STATUS "ENGINE sources: ${ENGINE_SOURCES}")

add_executable(SIMILI_UI WIN32
	${SIMILI_UI_SOURCES}
	${SIMILI_UI_HEADERS}
	${SIMILI_ROUTER_SOURCES}
	${SIMILI_ROUTER_HEADERS}
	${SIMILI_SERVER_SOURCES}
	${SIMILI_SERVER_HEADERS}
	${ENGINE_SOURCES}
	${ENGINE_HEADERS}
)

target_link_libraries(SIMILI_UI PRIVATE
	libcef_dll_wrapper
	${CEF_STANDARD_LIBS}
	${CEF_ROOT}/Release/libcef.lib
	glad
	opengl32
	glfw
	imgui
	tinyfiledialogs
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
	${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/imgui
	${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/imgui/backends
	${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/ImGuizmo
	${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/GLFW/include
	${CMAKE_CURRENT_SOURCE_DIR}/../../ThirdParty/tinyfiledialogs
	${CMAKE_CURRENT_SOURCE_DIR}/../..   
	${CMAKE_CURRENT_SOURCE_DIR}/../../Engine
	${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/ThreeDScene_DNA
	${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/ThreeDInteractions
	${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects
	${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Basic
	${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Camera
	${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Entities
	${CMAKE_CURRENT_SOURCE_DIR}/../../WorldObjects/Mesh_DNA
	${CMAKE_CURRENT_SOURCE_DIR}/../../UI
	${CMAKE_CURRENT_SOURCE_DIR}/../../UI/HierarchyInspectorLogic
	${CMAKE_CURRENT_SOURCE_DIR}/../../UI/ObjectInspectorLogic
	${CMAKE_CURRENT_SOURCE_DIR}/../../UI/ThreeDWindow
	${CMAKE_CURRENT_SOURCE_DIR}/../../UI/ThreeDModes
	${CMAKE_CURRENT_SOURCE_DIR}/../../UI/ContextualMenu
	${CMAKE_CURRENT_SOURCE_DIR}/../../UI/EdgeLoopControl
	${CMAKE_CURRENT_SOURCE_DIR}/../../UI/UIdocking
	${CMAKE_CURRENT_SOURCE_DIR}/../../UI/OptionMenu
	${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/MeshEdit
	${CMAKE_CURRENT_SOURCE_DIR}/../../Engine/SaveLoadSystem
	${Boost_INCLUDE_DIRS}
	${OPENSSL_INCLUDE_DIR}
)

target_compile_definitions(SIMILI_UI PRIVATE
	UNICODE
	_UNICODE
	WIN32_LEAN_AND_MEAN
	NOMINMAX
	_WIN32_WINNT=0x0601
	SIMILI_UI_NO_SDL=1
	GLM_ENABLE_EXPERIMENTAL
)


set(CEF_BINARY_DIR "${CEF_ROOT}/Release")
set(CEF_RESOURCE_DIR "${CEF_ROOT}/Resources")

install(TARGETS SIMILI_UI DESTINATION bin)
install(DIRECTORY ${CEF_BINARY_DIR}/ DESTINATION bin FILES_MATCHING PATTERN "*.dll")
install(DIRECTORY ${CEF_RESOURCE_DIR}/ DESTINATION bin)
