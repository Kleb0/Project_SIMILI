// CEF includes - MUST be first to avoid conflicts
#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_sandbox_win.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "SIMILI_Frontend/ui_handler.hpp"
#include "SIMILI_Frontend/simple_window_delegate.hpp"
#include "SIMILI_Frontend/simple_browser_view_delegate.hpp"
#include "SIMILI_Frontend/viewportLogic/Keymanagement/IFrameMouseDetector.hpp"

#include "SIMILI_Services/router/RouterSim.hpp"
#include "SIMILI_Services/router/RoutesManager.hpp"
#include "SIMILI_Services/types/RouterTypes.hpp"
#include "SIMILI_Services/middleware/SimpleHttpServer.hpp"
#include "Engine/OpenGLScene/OpenGLContext.hpp"
#include "Engine/OpenGLScene/ThreeDScene.hpp"
#include "Engine/VulkanScene/VKcontext.hpp"
#include "Engine/VulkanScene/VKScene.Hpp"
#include "WorldObjects/Camera/Camera.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include "Engine/PrimitivesCreation/CreatePrimitive.hpp"

#include <iostream>
#include <sstream>
#include <fstream>
#include <thread>
#include <chrono>
#include <filesystem>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>

#ifdef _MSC_VER
#pragma comment(linker, "/SUBSYSTEM:CONSOLE")
#endif

namespace fs = std::filesystem;
fs::path gExecutableDir;

int main(int argc, char* argv[])
{
	CefMainArgs main_args(GetModuleHandle(nullptr));

	CefRefPtr<UIHandler> handler(new UIHandler);

	int exit_code = CefExecuteProcess(main_args, handler, nullptr);
	if (exit_code >= 0) 
	{
		return exit_code;
	}

	const char* basePath = SDL_GetBasePath();
	if (basePath)
	{
		gExecutableDir = fs::path(basePath);
		SDL_free((void*)basePath);
	}
	else
	{
		gExecutableDir = fs::current_path();
	}

	std::cout << "[Main] Starting SIMILI with CEF..." << std::endl;
	std::cout << "[Main] Vulkan API version: " << VK_API_VERSION_1_0 << std::endl;

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
	{
		std::cerr << "[Main] Failed to initialize SDL3: " << SDL_GetError() << std::endl;
		return -1;
	}
	std::cout << "[Main] SDL3 initialized" << std::endl;

	if (!glfwInit()) 
	{
		std::cerr << "[Main] Failed to initialize GLFW" << std::endl;
		SDL_Quit();
		return -1;
	}
	std::cout << "[Main] GLFW initialized" << std::endl;

	// Create hidden GLFW window for OpenGL context
	glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	
	GLFWwindow* hidden_window = glfwCreateWindow(800, 600, "SIMILI_Hidden_GL_Context", nullptr, nullptr);
	if (!hidden_window) 
	{
		std::cerr << "[Main] Failed to create GLFW window for OpenGL context" << std::endl;
		glfwTerminate();
		return -1;
	}
	
	glfwMakeContextCurrent(hidden_window);
	
	// Initialize GLAD
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) 
	{
		std::cerr << "[Main] Failed to initialize GLAD" << std::endl;
		glfwDestroyWindow(hidden_window);
		glfwTerminate();
		return -1;
	}
	std::cout << "[Main] OpenGL " << glGetString(GL_VERSION) << " initialized" << std::endl;
	
	std::cout << "[Main] Starting HTTP Server..." << std::endl;
	try 
	{
		SIMILI::Server::SimpleHttpServer::getInstance().start(8080, 8443);
		std::cout << "[Main] HTTP Server started on port 8080" << std::endl;
	} 
	catch (const std::exception& e) {
		std::cerr << "[Main] ERROR starting HTTP Server: " << e.what() << std::endl;
		glfwDestroyWindow(hidden_window);
		glfwTerminate();
		return -1;
	}

	ThreeDScene myThreeDScene;
	Camera mainCamera;
	mainCamera.setName("MainCamera");
	mainCamera.initialize();

	Mesh* cubeMesh1 = Primitives::CreateCubeMesh(1.0f, glm::vec3(0.0f, 0.0f, 0.0f), "Cube", true);
	cubeMesh1->initialize();

	OpenGLContext renderer;
	std::cout << "[Main] OpenGL Context ID: " << renderer.getContextID() << std::endl;

	VKContext vkRenderer;
	vkRenderer.initialize();
	std::cout << "[Main] Instance of Vulkan created successfully !" << std::endl;

	VKScene myVKScene;
	myVKScene.setVKContext(&vkRenderer);
	myVKScene.initialize();
	myVKScene.setActiveCamera(&mainCamera);
	std::cout << "[Main] VKScene initialized with ID: " << myVKScene.getSceneID() << std::endl;

	myThreeDScene.setOpenGLContext(&renderer);
	myThreeDScene.initizalize();
	myThreeDScene.setActiveCamera(&mainCamera);
	
	myThreeDScene.addObject(cubeMesh1);
	myThreeDScene.addObject(&mainCamera);

	std::cout << "[Main] 3D Scene initialized with ID: " << myThreeDScene.getSceneID() << std::endl;

	// Initialize all API routes via RoutesManager
	auto& router = SIMILI::Server::SimpleHttpServer::getInstance().getRouter();
	SIMILI::Router::RoutesManager routesManager;
	routesManager.initializeRoutes(router, renderer, myThreeDScene, handler, hidden_window);
	std::cout << "[Main] All API routes initialized via RoutesManager" << std::endl;

	CefSettings settings;
	settings.no_sandbox = true;
	settings.multi_threaded_message_loop = false;
	settings.log_severity = LOGSEVERITY_DISABLE;

	if (!CefInitialize(main_args, settings, handler, nullptr)) 
	{
		std::cerr << "[Main] Failed to initialize CEF" << std::endl;
		return -1;
	}

	handler->setThreeDScene(&myThreeDScene);
	std::cout << "[Main] 3D Scene linked to UIHandler" << std::endl;

	handler->setSceneObjects(&renderer, &myThreeDScene, &mainCamera, &cubeMesh1);
	std::cout << "[Main] Scene objects passed to UIHandler" << std::endl;

	auto iframeMouseDetector = std::make_unique<SIMILI::Input::IFrameMouseDetector>();
	handler->setIFrameMouseDetector(iframeMouseDetector.get());
	std::cout << "[Main] IFrameMouseDetector created and linked to UIHandler" << std::endl;

	CefBrowserSettings browser_settings;
	browser_settings.windowless_frame_rate = 60;

	std::string url = "file:///ui/main_layout.html";

	CefRefPtr<SimpleBrowserViewDelegate> browser_view_delegate(new SimpleBrowserViewDelegate());

	CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
		handler, url, browser_settings, nullptr, nullptr, browser_view_delegate);

	CefRefPtr<SimpleWindowDelegate> window_delegate(new SimpleWindowDelegate(browser_view));
	handler->setWindowDelegate(window_delegate.get());
	window_delegate->setUIHandler(handler.get());
	
	// Initialize FrameDatas with window_delegate
	handler->initializeFrameDatas(window_delegate.get());
	
	CefWindow::CreateTopLevelWindow(window_delegate);


	std::cout << "[Main] CEF window created, entering message loop..." << std::endl;
	CefRunMessageLoop();

	std::cout << "[Main] Shutting down CEF..." << std::endl;
	CefShutdown();

	glfwDestroyWindow(hidden_window);
	glfwTerminate();
	std::cout << "[Main] GLFW terminated" << std::endl;

	SDL_Quit();
	std::cout << "[Main] SDL3 terminated" << std::endl;

	return 0;
}