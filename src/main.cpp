// CEF includes - MUST be first to avoid conflicts
#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_sandbox_win.h"
#include "SIMILI_Frontend/ui_handler.hpp"
#include "SIMILI_Frontend/SDL_ApplicationWindow.hpp"
#include "SIMILI_Frontend/viewportLogic/overlay_viewport.hpp"
#include "SIMILI_Frontend/viewportLogic/FrameDatas/FrameDatas.hpp"
#include "SIMILI_Frontend/viewportLogic/ThreeDScreen/ThreeDScreen.hpp"

#include "SIMILI_Services/router/RouterSim.hpp"
#include "SIMILI_Services/router/RoutesManager.hpp"
#include "SIMILI_Services/types/RouterTypes.hpp"
#include "SIMILI_Services/middleware/SimpleHttpServer.hpp"
#include "Engine/VulkanScene/VKcontext.hpp"
#include "Engine/VulkanScene/VKScene.Hpp"
#include "Engine/SceneObjectContainer/SceneObjectContainer.hpp"
#include "Engine/VulkanPipeline/VulkanPipeline.hpp"
#include "WorldObjects/Camera/Camera.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include "Engine/PrimitivesCreation/CreatePrimitive.hpp"
#include "DebugLogger.hpp"

#include <iostream>
#include <sstream>
#include <fstream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <cstring>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#ifdef _MSC_VER
#pragma comment(linker, "/SUBSYSTEM:CONSOLE")
#endif

namespace fs = std::filesystem;
fs::path gExecutableDir;

fs::path resolveUiLayoutPath()
{
	std::vector<fs::path> searchRoots;
	searchRoots.push_back(gExecutableDir);

	fs::path currentRoot = gExecutableDir;
	for (int depth = 0; depth < 4; ++depth)
	{
		if (currentRoot.empty() || !currentRoot.has_parent_path())
		{
			break;
		}

		currentRoot = currentRoot.parent_path();
		searchRoots.push_back(currentRoot);
	}

	searchRoots.push_back(fs::current_path());

	for (const auto& root : searchRoots)
	{
		if (root.empty())
		{
			continue;
		}

		fs::path candidate = (root / "ui" / "main_layout.html").lexically_normal();
		if (fs::exists(candidate))
		{
			return fs::weakly_canonical(candidate);
		}
	}

	return (gExecutableDir / "ui" / "main_layout.html").lexically_normal();
}

int main(int argc, char* argv[])
{
	CefMainArgs main_args(GetModuleHandle(nullptr));

	CefRefPtr<UIHandler> handler(new UIHandler);

	int exit_code = CefExecuteProcess(main_args, handler, nullptr);
	if (exit_code >= 0) 
	{
		return exit_code;
	}

	DualLogger logger;
	const char* basePath = SDL_GetBasePath();
	std::string logFilePath;

	if (basePath) 
	{
		logFilePath = std::string(basePath) + "debug_output.txt";
		gExecutableDir = fs::path(basePath);
		SDL_free((void*)basePath);
	} 
	else
	{
		logFilePath = "debug_output.txt";
		gExecutableDir = fs::current_path();
	}
	
	if (!logger.initialize(logFilePath)) {
		std::cerr << "[Main] Warning: Failed to initialize file logging" << std::endl;
	} 
	else
	{
		std::cout << "[Main] Debug logging enabled to: " << logFilePath << std::endl;
	}

	OverlayViewport ThreeDViewport;

	std::cout << "[Main] Starting SIMILI with CEF..." << std::endl;
	std::cout << "[Main] Vulkan API version: " << VK_API_VERSION_1_0 << std::endl;

	SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "windows");
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d11,direct3d12,opengl,vulkan");

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
	{
		std::cerr << "[Main] Failed to initialize SDL3: " << SDL_GetError() << std::endl;
		std::cerr << "[Main] Available video drivers:" << std::endl;
		int numDrivers = SDL_GetNumVideoDrivers();
		for (int i = 0; i < numDrivers; i++)
		{
			std::cerr << "  " << i << ": " << SDL_GetVideoDriver(i) << std::endl;
		}
		return -1;
	}
	std::cout << "[Main] SDL3 initialized" << std::endl;
	std::cout << "[Main] Using video driver: " << SDL_GetCurrentVideoDriver() << std::endl;
	
	std::cout << "[Main] Loading Vulkan library..." << std::endl;
	if (!SDL_Vulkan_LoadLibrary(nullptr))
	{
		std::cerr << "[Main] Failed to load Vulkan library: " << SDL_GetError() << std::endl;
		SDL_Quit();
		return -1;
	}
	std::cout << "[Main] Vulkan library loaded" << std::endl;
	
	std::cout << "[Main] Creating SDL Application Window..." << std::endl;
	SDL_ApplicationWindow mainWindow;

	std::cout << "[Main] Calling mainWindow.create()..." << std::endl;
	if (!mainWindow.create("SIMILI PROJECT", 1920, 1080, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY))
	{
		std::cerr << "[Main] Failed to create SDL application window" << std::endl;
		SDL_Quit();
		return -1;
	}
	std::cout << "[Main] SDL Application Window created" << std::endl;
	
	VKContext vkRenderer;
	vkRenderer.initialize(mainWindow.getHandle());
	std::cout << "[Main] VKContext initialized" << std::endl;
	
	// Create and initialize Vulkan pipeline system
	VulkanPipeline vulkanPipelines;
	vulkanPipelines.initialize(&vkRenderer);
	std::cout << "[Main] VulkanPipeline initialized" << std::endl;
	
	// Set VulkanPipeline on window for distribution to components
	mainWindow.setVulkanPipelines(&vulkanPipelines);
	std::cout << "[Main] VulkanPipeline set on window" << std::endl;
	
	handler->setVKRenderer(&vkRenderer);
	std::cout << "[Main] VKRenderer linked to UIHandler" << std::endl;
	
	handler->setVulkanPipelines(mainWindow.getVulkanPipelines());
	std::cout << "[Main] VulkanPipeline linked to UIHandler" << std::endl;
	
	ThreeDScreen myThreeDScreen;
	myThreeDScreen.initialize();
	mainWindow.setThreeDScreen(&myThreeDScreen);
	vkRenderer.setThreeDScreen(&myThreeDScreen);
	std::cout << "[Main] ThreeDScreen initialized and linked to SDL_ApplicationWindow" << std::endl;
	
	handler->Set_SDLParent(&mainWindow);
	mainWindow.Set_UIHandler(handler.get());
	
	auto* frameDatas = new SIMILI::Frontend::FrameDatas(handler.get());
	handler->setFrameDatas(frameDatas);
	std::cout << "[Main] FrameDatas created and linked to UIHandler" << std::endl;

	std::cout << "[Main] Creating UIManager..." << std::endl;
	SIMILI::Frontend::UIManager myUIManager;
	mainWindow.setUIManager(&myUIManager);
	handler->setUIManager(&myUIManager);
	std::cout << "[Main] UIManager created and linked to SDL_ApplicationWindow and UIHandler" << std::endl;
	
	SDL_StartTextInput(mainWindow.getHandle());
	
	std::cout << "[Main] Starting HTTP Server..." << std::endl;

	SIMILI::Server::SimpleHttpServer::getInstance().start(8080, 8443);
	std::cout << "[Main] HTTP Server started on port 8080" << std::endl;

	std::cout << "[Main] Creating Camera..." << std::endl;
	Camera mainCamera;
	std::cout << "[Main] Camera created" << std::endl;
	
	mainCamera.setName("MainCamera");
	std::cout << "[Main] Camera name set" << std::endl;
	
	mainCamera.initialize();
	std::cout << "[Main] Camera initialized" << std::endl;

	std::cout << "[Main] Creating cube mesh..." << std::endl;
	// TEMPORARY: Comment out mesh creation to isolate CEF issue
	// Mesh* cubeMesh1 = Primitives::CreateCubeMesh(1.0f, glm::vec3(0.0f, 0.0f, 0.0f), "Cube", true);
	// std::cout << "[Main] Cube mesh created" << std::endl;
	// 
	// cubeMesh1->initialize();
	// std::cout << "[Main] Cube mesh initialized" << std::endl;

	std::cout << "[Main] Setting VKContext on window..." << std::endl;
	mainWindow.setVKContext(&vkRenderer);
	std::cout << "[Main] VKContext set on window" << std::endl;
	
	std::cout << "[Main] Initializing Vulkan for window..." << std::endl;
	if (!mainWindow.initializeVulkan())
	{
		std::cerr << "[Main] Failed to initialize Vulkan for window" << std::endl;
		mainWindow.destroy();
		SDL_Quit();
		return -1;
	}
	std::cout << "[Main] Vulkan initialized for SDL window" << std::endl;

	VkRenderPass renderPass = mainWindow.getRenderPass();
	std::cout << "[Main] Retrieved render pass: " << renderPass << std::endl;
	
	if (renderPass == VK_NULL_HANDLE)
	{
		std::cerr << "[Main] ERROR: Render pass is VK_NULL_HANDLE!" << std::endl;
		mainWindow.destroy();
		SDL_Quit();
		return -1;
	}

	mainWindow.setRenderPass(renderPass);
	std::cout << "[Main] Render pass configured on SDL_ApplicationWindow" << std::endl;


	myThreeDScreen.setVKContext(&vkRenderer);

	SceneObjectContainer VKSceneObjectContainer;

	VKScene myVKScene;
	myVKScene.setVKContext(&vkRenderer);
	myVKScene.setSceneObjectContainer(&VKSceneObjectContainer);
	myVKScene.initialize();
	// TEMPORARY: Comment out mesh addition while mesh creation is disabled
	// myVKScene.addObject(cubeMesh1);
	myVKScene.addObject(&mainCamera);

	Camera* sceneCamera = nullptr;

	for (auto* obj : VKSceneObjectContainer.getObjectsRef())
	{
		Camera* cam = dynamic_cast<Camera*>(obj);
		if (cam && cam->getName() == "MainCamera")
		{
			sceneCamera = cam;
			std::cout << "[Main] Found main camera in scene: " << cam->getName() << std::endl;
			break;
		}
	}

	if (sceneCamera)
	{
		myVKScene.setCameraToUse(sceneCamera);
		myThreeDScreen.setCamera(myVKScene.getActiveCamera());
		ThreeDViewport.setCamera(myVKScene.getActiveCamera());
		vkRenderer.setCamera(sceneCamera);
		vkRenderer.setThreeDScreen(&myThreeDScreen);
		sceneCamera->setVKScene(&myVKScene);
		std::cout << "[Main] Camera linked to VKContext" << std::endl;
	}

	std::cout << "[Main] VKScene initialized with ID: " << myVKScene.getSceneID() << std::endl;

	auto& router = SIMILI::Server::SimpleHttpServer::getInstance().getRouter();
	SIMILI::Router::RoutesManager routesManager;
	routesManager.initializeRoutes(router, vkRenderer, myVKScene, handler, nullptr);
	std::cout << "[Main] All API routes initialized via RoutesManager" << std::endl;

	CefSettings settings;
	settings.no_sandbox = true;
	settings.multi_threaded_message_loop = false;
	settings.log_severity = LOGSEVERITY_DISABLE;
	settings.remote_debugging_port = 9222;
	
	CefString(&settings.user_agent).FromASCII("SimiliCEF/1.0");
	
	std::cout << "[Main] CEF Settings configured with localhost access enabled" << std::endl;

	if (!CefInitialize(main_args, settings, handler, nullptr)) 
	{
		std::cerr << "[Main] Failed to initialize CEF" << std::endl;
		return -1;
	}

	handler->setVKScene(&myVKScene);
	std::cout << "[Main] VKScene linked to UIHandler" << std::endl;

	mainWindow.setVKScene(&myVKScene);
	std::cout << "[Main] VKScene linked to SDL_ApplicationWindow" << std::endl;

	handler->startRenderTimer();
	std::cout << "[Main] Render timer started" << std::endl;

	mainWindow.show();
	std::cout << "[Main] SDL window shown before CEF creation" << std::endl;

	SDL_Event dummyEvent;
	for (int i = 0; i < 10; ++i)
	{
		while (SDL_PollEvent(&dummyEvent));
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	int windowWidth = 0;
	int windowHeight = 0;
	int windowPixelWidth = 0;
	int windowPixelHeight = 0;
	SDL_GetWindowSize(mainWindow.getHandle(), &windowWidth, &windowHeight);
	SDL_GetWindowSizeInPixels(mainWindow.getHandle(), &windowPixelWidth, &windowPixelHeight);
	std::cout << "[Main] Window size after show: " << windowWidth << "x" << windowHeight << " (pixels: " << windowPixelWidth << "x" << windowPixelHeight << ")" << std::endl;

	fs::path uiPath = resolveUiLayoutPath();
	if (!fs::exists(uiPath))
	{
		std::cerr << "[Main] UI layout not found: " << uiPath.string() << std::endl;
		CefShutdown();
		mainWindow.cleanupVulkan();
		mainWindow.destroy();
		SDL_Quit();
		return -1;
	}
	std::cout << "[Main] Resolved UI layout path: " << uiPath.string() << std::endl;
	
	std::string url = "http://localhost:8080/ui/main_layout.html";
	mainWindow.SetHTMLAdressToDraw(handler, url, windowWidth, windowHeight);
	std::cout << "[Main] CEF browser created successfully" << std::endl;
	
	std::cout << "[Main] Waiting for CEF and JavaScript initialization..." << std::endl;
	for (int i = 0; i < 120; ++i)
	{
		CefDoMessageLoopWork();
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}
	std::cout << "[Main] CEF initialization wait complete" << std::endl;
	
	handler->initializeDefaultUIPanels();
	std::cout << "[Main] Default UI panels initialized" << std::endl;

	handler->forceCaptureIFramePositions();
	std::cout << "[Main] Initial frame data captured" << std::endl;


	// Needs to be changed
	mainWindow.startSplitter();
	std::cout << "[Main] Splitter started and UIManager configured" << std::endl;

	std::cout << "[Main] SDL window shown" << std::endl;

	// Main render loop
	std::cout << "[Main] Starting main render loop..." << std::endl;
	std::cout << "[Main] Swapchain state before loop: " << (mainWindow.getSwapchain() != VK_NULL_HANDLE) 
	          << " (handle=" << mainWindow.getSwapchain() << ")" << std::endl;
	bool running = true;
	SDL_Event event;
	
	while (running)
	{
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_EVENT_QUIT)
			{
				running = false;
			}
			
			mainWindow.handleSplitterEvent(event);
		}
		
		mainWindow.processEvents();
		
		CefDoMessageLoopWork();
		
		mainWindow.renderFrame();
	}

	std::cout << "[Main] Shutting down..." << std::endl;
	
	if (vkRenderer.getDevice() != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(vkRenderer.getDevice());
	}
	
	std::cout << "[Main] Shutting down CEF..." << std::endl;
	handler->clearUIPanels();
	CefShutdown();

	mainWindow.cleanupVulkan();
	std::cout << "[Main] Vulkan cleaned up" << std::endl;

	mainWindow.destroy();
	
	std::cout << "[Main] Unloading Vulkan library..." << std::endl;
	SDL_Vulkan_UnloadLibrary();
	std::cout << "[Main] Vulkan library unloaded" << std::endl;
	
	SDL_Quit();
	std::cout << "[Main] SDL3 terminated" << std::endl;

	return 0;
}