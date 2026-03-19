// CEF includes - MUST be first to avoid conflicts
#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_sandbox_win.h"
#include "SIMILI_Frontend/ui_handler.hpp"
#include "SIMILI_Frontend/SDL_ApplicationWindow.hpp"
#include "SIMILI_Frontend/CEF_Drawer.hpp"
#include "SIMILI_Frontend/viewportLogic/Keymanagement/MouseController.hpp"
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

	auto MouseControl = new SIMILI::Input::MouseController();
	handler->set_MouseControl(MouseControl);

	OverlayViewport ThreeDViewport;

	handler->set_Overlay_Viewport(&ThreeDViewport);

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
	
	// Create main SDL window
	SDL_ApplicationWindow mainWindow;

	if (!mainWindow.create("SIMILI PROJECT", 1920, 1080))
	{
		std::cerr << "[Main] Failed to create SDL application window" << std::endl;
		SDL_Quit();
		return -1;
	}
	std::cout << "[Main] SDL Application Window created" << std::endl;

	// Create OpenGL context for SDL window
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	
	SDL_GLContext glContext = SDL_GL_CreateContext(mainWindow.getHandle());
	if (!glContext)
	{
		std::cerr << "[Main] Failed to create OpenGL context: " << SDL_GetError() << std::endl;
		mainWindow.destroy();
		SDL_Quit();
		return -1;
	}
	
	SDL_GL_MakeCurrent(mainWindow.getHandle(), glContext);
	SDL_GL_SetSwapInterval(1); // Enable vsync
	
	// Initialize GLAD with SDL
	if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
	{
		std::cerr << "[Main] Failed to initialize GLAD" << std::endl;
		SDL_GL_DestroyContext(glContext);
		mainWindow.destroy();
		SDL_Quit();
		return -1;
	}
	std::cout << "[Main] OpenGL " << glGetString(GL_VERSION) << " initialized" << std::endl;
	
	// NOW initialize CEF drawer (requires active OpenGL context)
	CefRefPtr<CEF_Drawer> cefDrawer(new CEF_Drawer());
	if (!cefDrawer->initialize(mainWindow.getHandle()))
	{
		std::cerr << "[Main] Failed to initialize CEF Drawer" << std::endl;
		SDL_GL_DestroyContext(glContext);
		mainWindow.destroy();
		SDL_Quit();
		return -1;
	}
	handler->setCEFDrawer(cefDrawer.get());
	std::cout << "[Main] CEF Drawer initialized" << std::endl;
	
	handler->Set_SDLParent(&mainWindow);
	handler->Set_DOM(cefDrawer.get());
	mainWindow.Set_UIHandler(handler.get());
	
	auto* frameDatas = new SIMILI::Frontend::FrameDatas(handler.get());
	handler->setFrameDatas(frameDatas);
	std::cout << "[Main] FrameDatas created and linked to UIHandler" << std::endl;
	
	ThreeDScreen myThreeDScreen;
	myThreeDScreen.initialize();
	mainWindow.setThreeDScreen(&myThreeDScreen);
	mainWindow.startSplitter();
	std::cout << "[Main] ThreeDScreen initialized and linked to SDL_ApplicationWindow" << std::endl;
	
	SDL_StartTextInput(mainWindow.getHandle());
	
	std::cout << "[Main] Starting HTTP Server..." << std::endl;

	SIMILI::Server::SimpleHttpServer::getInstance().start(8080, 8443);
	std::cout << "[Main] HTTP Server started on port 8080" << std::endl;

	Camera mainCamera;
	mainCamera.setName("MainCamera");
	mainCamera.initialize();

	Mesh* cubeMesh1 = Primitives::CreateCubeMesh(1.0f, glm::vec3(0.0f, 0.0f, 0.0f), "Cube", true);
	cubeMesh1->initialize();

	VKContext vkRenderer;
	vkRenderer.initialize();

	SceneObjectContainer VKSceneObjectContainer;

	VKScene myVKScene;
	myVKScene.setVKContext(&vkRenderer);
	myVKScene.setSceneObjectContainer(&VKSceneObjectContainer);
	myVKScene.initialize();
	myVKScene.addObject(cubeMesh1);
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
		ThreeDViewport.setCamera(myVKScene.getActiveCamera());
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

	if (!CefInitialize(main_args, settings, handler, nullptr)) 
	{
		std::cerr << "[Main] Failed to initialize CEF" << std::endl;
		return -1;
	}

	handler->setVKScene(&myVKScene);
	std::cout << "[Main] VKScene linked to UIHandler" << std::endl;

	handler->startRenderTimer();
	std::cout << "[Main] Render timer started" << std::endl;

	int windowWidth = 0;
	int windowHeight = 0;
	int windowPixelWidth = 0;
	int windowPixelHeight = 0;
	SDL_GetWindowSize(mainWindow.getHandle(), &windowWidth, &windowHeight);
	SDL_GetWindowSizeInPixels(mainWindow.getHandle(), &windowPixelWidth, &windowPixelHeight);
	std::cout << "[Main] Window pixel size: " << windowPixelWidth << "x" << windowPixelHeight << std::endl;

	fs::path uiPath = fs::absolute(gExecutableDir / "ui" / "main_layout.html").lexically_normal();
	std::string url = "file:///" + uiPath.generic_string();
	if (!cefDrawer->createBrowser(handler, url, windowWidth, windowHeight))
	{
		std::cerr << "[Main] Failed to create CEF browser" << std::endl;
		CefShutdown();
		mainWindow.destroy();
		return -1;
	}

	// Show SDL window
	mainWindow.show();
	handler->captureIFramePositions();
	std::cout << "[Main] SDL window shown" << std::endl;

	// Main render loop
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
			
			if (!mainWindow.handleSplitterEvent(event))
			{
				cefDrawer->handleEvent(event);
			}
		}
		
		mainWindow.processEvents();
		
		CefDoMessageLoopWork();
		cefDrawer->syncWindowProperties();
		
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		
		mainWindow.drawThreeDScreen();
		mainWindow.drawUIPanels();
		
		SDL_GL_SwapWindow(mainWindow.getHandle());
	}

	std::cout << "[Main] Shutting down CEF..." << std::endl;
	handler->clearUIPanels();
	CefShutdown();

	SDL_GL_DestroyContext(glContext);
	std::cout << "[Main] OpenGL context destroyed" << std::endl;

	SDL_Quit();
	std::cout << "[Main] SDL3 terminated" << std::endl;

	return 0;
}