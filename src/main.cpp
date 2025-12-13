// CEF includes - MUST be first to avoid conflicts
#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_sandbox_win.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "SIMILI_Frontend/UI_Engine/ui_handler.hpp"
#include "SIMILI_Frontend/UI_Engine/simple_window_delegate.hpp"
#include "SIMILI_Frontend/UI_Engine/simple_browser_view_delegate.hpp"

#include "SIMILI_Services/router/RouterSim.hpp"
#include "SIMILI_Services/types/RouterTypes.hpp"
#include "SIMILI_Services/middleware/SimpleHttpServer.hpp"
#include "Engine/OpenGLContext.hpp"
#include "Engine/ThreeDScene.hpp"
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

namespace fs = std::filesystem;
fs::path gExecutableDir;

#ifdef _WIN32
#include <windows.h>

BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
	if (dwCtrlType == CTRL_CLOSE_EVENT || dwCtrlType == CTRL_C_EVENT || 
		dwCtrlType == CTRL_BREAK_EVENT || dwCtrlType == CTRL_LOGOFF_EVENT || 
		dwCtrlType == CTRL_SHUTDOWN_EVENT) {
		
		std::cout << "[ConsoleCtrl] Cleanup signal received..." << std::endl;
		return TRUE;
	}
	return FALSE;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	CefMainArgs main_args(hInstance);
	CefRefPtr<UIHandler> handler(new UIHandler);

	int exit_code = CefExecuteProcess(main_args, handler, nullptr);
	if (exit_code >= 0) 
	{
		return exit_code;
	}

	static bool console_allocated = false;
	if (!console_allocated) 
	{
		AllocConsole();
		FILE* fp;
		freopen_s(&fp, "CONOUT$", "w", stdout);
		freopen_s(&fp, "CONOUT$", "w", stderr);
		console_allocated = true;
	}

	wchar_t exePath[MAX_PATH];
	GetModuleFileNameW(NULL, exePath, MAX_PATH);
	gExecutableDir = fs::path(exePath).parent_path();

	SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

	std::cout << "[Main] Starting SIMILI with CEF..." << std::endl;

	if (!glfwInit()) 
	{
		std::cerr << "[Main] Failed to initialize GLFW" << std::endl;
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

	myThreeDScene.setOpenGLContext(&renderer);
	myThreeDScene.initizalize();
	myThreeDScene.setActiveCamera(&mainCamera);
	
	myThreeDScene.addObject(cubeMesh1);
	myThreeDScene.addObject(&mainCamera);

	std::cout << "[Main] 3D Scene initialized with ID: " << myThreeDScene.getSceneID() << std::endl;

	auto& router = SIMILI::Server::SimpleHttpServer::getInstance().getRouter();
	
	router.get("/api/context", [&renderer](const SIMILI::Router::Message& msg) -> SIMILI::Router::Response 
	{
		SIMILI::Router::Response resp;
		resp.statusCode = 200;
		resp.statusMessage = "OK";
		resp.body = "{\"contextId\": \"" + renderer.getContextID() + "\"}";
		resp.headers["Content-Type"] = "application/json";
		return resp;
	}, "Get OpenGL context ID");
	
	// Route: Get scene objects
	router.get("/api/scene/objects", [&myThreeDScene](const SIMILI::Router::Message& msg) -> SIMILI::Router::Response {
		SIMILI::Router::Response resp;
		resp.statusCode = 200;
		resp.statusMessage = "OK";
		
		std::ostringstream json;
		json << "[";
		
		auto& objects = myThreeDScene.getObjectsRef();
		bool first = true;

		for (auto* obj : objects) 
		{
			if (!first) json << ",";
			
			std::string objType = "Unknown";
			if (dynamic_cast<Camera*>(obj)) 
			{
				objType = "Camera";
			} 
			else if (obj->getIsMesh()) 
			{
				objType = "Mesh";
			}
			
			json << "{"
				 << "\"id\":" << obj->getID() << ","
				 << "\"name\":\"" << obj->getName() << "\","
				 << "\"type\":\"" << objType << "\","
				 << "\"selected\":" << (obj->getSelected() ? "true" : "false")
				 << "}";
			first = false;
		}
		
		json << "]";
		resp.body = json.str();
		resp.headers["Content-Type"] = "application/json";
		return resp;
	}, "Get all scene objects");
	
	// Route: Get scene info (Scene ID + Context ID)
	router.get("/api/scene-info", [&myThreeDScene, &renderer](const SIMILI::Router::Message& msg) -> SIMILI::Router::Response {
		SIMILI::Router::Response resp;
		resp.statusCode = 200;
		resp.statusMessage = "OK";
		
		std::ostringstream json;
		json << "{"
			 << "\"sceneID\":\"" << myThreeDScene.getSceneID() << "\","
			 << "\"contextID\":\"" << renderer.getContextID() << "\""
			 << "}";
		
		resp.body = json.str();
		resp.headers["Content-Type"] = "application/json";
		resp.headers["Access-Control-Allow-Origin"] = "*";
		return resp;
	}, "Get scene and context IDs");
	
	router.post("/api/create-cube", [&myThreeDScene, &handler, hidden_window](const SIMILI::Router::Message& msg) -> SIMILI::Router::Response {
		
		static int cubeCounter = 2; 
		
		std::string cubeName = "Cube" + std::to_string(cubeCounter);

		std::cout << "\n[Main] Creating new cube: " << cubeName << std::endl;
		
		float spacing = 2.0f;
		glm::vec3 position((cubeCounter - 1) * spacing, 0.0f, 0.0f);
		cubeCounter++;

		glfwMakeContextCurrent(nullptr);
		
		Mesh* newCube = Primitives::CreateCubeMesh(1.0f, position, cubeName, true);
		
		if (!newCube) 
		{
			SIMILI::Router::Response resp;
			resp.statusCode = 500;
			resp.statusMessage = "Internal Server Error";
			resp.body = "{\"error\": \"Failed to create cube\"}";
			resp.headers["Content-Type"] = "application/json";
			resp.headers["Access-Control-Allow-Origin"] = "*";
			return resp;
		}
		
		
		myThreeDScene.addObject(newCube);
		
		if (handler) 
		{
			handler->reinitializeSingleObject(newCube);
			handler->notifySceneChanged();
		}

		
		SIMILI::Router::Response resp;
		resp.statusCode = 200;
		resp.statusMessage = "OK";
		resp.body = "{\"success\": true, \"cubeName\": \"" + cubeName + "\", \"id\": " + std::to_string(newCube->getID()) + "}";
		resp.headers["Content-Type"] = "application/json";
		resp.headers["Access-Control-Allow-Origin"] = "*";
		return resp;
	}, " Create a new cube and add it to the scene \n ");
	
	router.post("/api/select-object", [&myThreeDScene, &handler](const SIMILI::Router::Message& msg) -> SIMILI::Router::Response {
		SIMILI::Router::Response resp;
		resp.headers["Content-Type"] = "application/json";
		resp.headers["Access-Control-Allow-Origin"] = "*";
		
		// Parse JSON without exceptions
		auto requestData = nlohmann::json::parse(msg.body, nullptr, false);
		if (requestData.is_discarded()) 
		{
			std::cerr << "[Main] Error: Invalid JSON in request body" << std::endl;
			resp.statusCode = 400;
			resp.statusMessage = "Bad Request";
			resp.body = "{\"error\": \"Invalid JSON\"}";
			return resp;
		}
		
		// Validate required fields
		if (!requestData.contains("slotIndex") || !requestData.contains("objectName") || !requestData.contains("objectType")) {
			std::cerr << "[Main] Error: Missing required fields" << std::endl;
			resp.statusCode = 400;
			resp.statusMessage = "Bad Request";
			resp.body = "{\"error\": \"Missing required fields\"}";
			return resp;
		}
		
		int slotIndex = requestData["slotIndex"];
		std::string objectName = requestData["objectName"];
		std::string objectType = requestData["objectType"];
		bool shiftKey = requestData.value("shiftKey", false);
		
		std::cout << "[Main] Selection request - Slot: " << slotIndex 
				  << " | Object: " << objectName 
				  << " | Type: " << objectType 
				  << " | Shift: " << (shiftKey ? "YES" : "NO") << std::endl;
		
		auto& objects = myThreeDScene.getObjectsRef();
		
		if (slotIndex < 0 || slotIndex >= static_cast<int>(objects.size())) {
			resp.statusCode = 400;
			resp.statusMessage = "Bad Request";
			resp.body = "{\"error\": \"Invalid slot index\"}";
			return resp;
		}
		
		auto it = objects.begin();
		std::advance(it, slotIndex);
		ThreeDObject* selectedObject = *it;
		
		if (!selectedObject) 
		{
			resp.statusCode = 404;
			resp.statusMessage = "Not Found";
			resp.body = "{\"error\": \"Object not found\"}";
			return resp;
		}
		
		// If Shift key is NOT pressed, deselect all objects
		if (!shiftKey) 
		{
			for (auto* obj : objects) 
			{
				if (obj) obj->setSelected(false);
			}
		}
		
		bool isCamera = (objectType == "Camera");

		if (!isCamera) 
		{
			selectedObject->setSelected(true);
			
			if (handler && handler->getOverlay()) 
			{
				// Build list of ALL currently selected objects
				std::list<ThreeDObject*> selectedList;
				for (auto* obj : objects) 
				{
					if (obj && obj->getSelected()) 
					{
						selectedList.push_back(obj);
					}
				}
				
				handler->getOverlay()->setMultipleSelectedObjects(selectedList);
				
				// Force immediate high-priority redraw
				HWND overlayHwnd = handler->getOverlay()->getHandle();
				if (overlayHwnd) 
				{
					RedrawWindow(overlayHwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOCHILDREN);
				}
				
				std::cout << "[Main] " << selectedList.size() << " object(s) selected, gizmo render forced" << std::endl;
			}
		} 
		else 
		{
			std::cout << "[Main] Camera clicked - no gizmo displayed" << std::endl;
		}
		
		resp.statusCode = 200;
		resp.statusMessage = "OK";
		resp.body = "{\"success\": true, \"selected\": \"" + selectedObject->getName() + "\", \"isCamera\": " + (isCamera ? "true" : "false") + "}";
		return resp;
	}, "Select object from hierarchy inspector");
	
	std::cout << "[Main] API routes registered (/api/context, /api/scene/objects, /api/scene-info, /api/create-cube, /api/select-object)" << std::endl;

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

	CefBrowserSettings browser_settings;
	browser_settings.windowless_frame_rate = 60;

	std::string url = "file:///ui/main_layout.html";

	CefRefPtr<SimpleBrowserViewDelegate> browser_view_delegate(new SimpleBrowserViewDelegate());
	CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
		handler, url, browser_settings, nullptr, nullptr, browser_view_delegate);

	CefRefPtr<SimpleWindowDelegate> window_delegate(new SimpleWindowDelegate(browser_view));
	CefWindow::CreateTopLevelWindow(window_delegate);


	std::cout << "[Main] CEF window created, entering message loop..." << std::endl;
	CefRunMessageLoop();

	std::cout << "[Main] Shutting down CEF..." << std::endl;
	CefShutdown();

	glfwDestroyWindow(hidden_window);
	glfwTerminate();
	std::cout << "[Main] GLFW terminated" << std::endl;

	return 0;
}
#endif
	