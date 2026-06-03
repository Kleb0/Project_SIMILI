#include "RoutesManager.hpp"
#include "../ThirdParty/json.hpp"
#include "../../WorldObjects/Mesh/Mesh.hpp"
#include "../../WorldObjects/Camera/Camera.hpp"
#include "../../Engine/PrimitivesCreation/CreatePrimitive.hpp"
#include "../../SIMILI_Frontend/viewportLogic/UIPanels/FrameDataCatcher.hpp"
#include "../../SIMILI_Frontend/viewportLogic/UIPanels/UIManager.hpp"
#include "../../SIMILI_Frontend/viewportLogic/UIPanels/DataHolders.hpp"
#include "../../SIMILI_Frontend/SDL_ApplicationWindow.hpp"
#include <iostream>
#include <fstream>
#include <GLFW/glfw3.h>

#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;

namespace SIMILI {
	namespace Router {

		void RoutesManager::initializeRoutes(
			RouterSim& router,
			VKContext& vkRenderer,
			VKScene& scene,
			GLFWwindow* glfwWindow,
			FrameDataCatcher* frameCatcher,
			SIMILI::Frontend::UIManager* uiManager,
			SDL_ApplicationWindow* sdlWindow)
		{
			std::cout << "[RoutesManager] Initializing all routes..." << std::endl;
			
			registerContextRoutes(router, vkRenderer);
			registerSceneRoutes(router, scene, vkRenderer);
			registerObjectRoutes(router, scene, glfwWindow, sdlWindow);
			registerIFrameRoutes(router, frameCatcher, uiManager);			
			std::cout << "[RoutesManager] All routes registered successfully" << std::endl;
		}


		void RoutesManager::registerContextRoutes(RouterSim& router, VKContext& vkRenderer)
		{
			router.get("/api/context", [&vkRenderer](const Message& msg) -> Response 
			{
				Response resp;
				resp.statusCode = 200;
				resp.statusMessage = "OK";
				resp.body = "{\"contextId\": \"" + vkRenderer.getContextID() + "\"}";
				resp.headers["Content-Type"] = "application/json";
				return resp;
			}, "Get Vulkan context ID");
			
			std::cout << "[RoutesManager] Context routes registered" << std::endl;
		}


		void RoutesManager::registerSceneRoutes(RouterSim& router, VKScene& scene, VKContext& vkRenderer)
		{
			router.get("/api/scene/objects", [&scene](const Message& msg) -> Response 
			{
				Response resp;
				resp.statusCode = 200;
				resp.statusMessage = "OK";
				
				std::ostringstream json;
				json << "[";
				
				auto& objects = scene.getObjectsRef();
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
			
			router.get("/api/scene-info", [&scene, &vkRenderer](const Message& msg) -> Response 
			{
				Response resp;
				resp.statusCode = 200;
				resp.statusMessage = "OK";
				
				std::ostringstream json;
				json << "{"
					<< "\"sceneID\":\"" << scene.getSceneID() << "\","
					<< "\"contextID\":\"" << vkRenderer.getContextID() << "\""
					<< "}";
				
				resp.body = json.str();
				resp.headers["Content-Type"] = "application/json";
				resp.headers["Access-Control-Allow-Origin"] = "*";
				return resp;
			}, "Get scene and context IDs");
			
			std::cout << "[RoutesManager] Scene routes registered" << std::endl;
		}


		void RoutesManager::registerObjectRoutes(RouterSim& router, VKScene& scene, GLFWwindow* glfwWindow, SDL_ApplicationWindow* sdlWindow)
		{
			router.post("/api/create-cube", [&scene, glfwWindow](const Message& msg) -> Response 
			{
				static int cubeCounter = 2; 
				
				std::string cubeName = "Cube" + std::to_string(cubeCounter);
				std::cout << "\n[RoutesManager] Creating new cube: " << cubeName << std::endl;
				
				float spacing = 2.0f;
				glm::vec3 position((cubeCounter - 1) * spacing, 0.0f, 0.0f);
				cubeCounter++;

				glfwMakeContextCurrent(nullptr);
				
				Mesh* newCube = Primitives::CreateCubeMesh(1.0f, position, cubeName, true);
				
				if (!newCube) 
				{
					Response resp;
					resp.statusCode = 500;
					resp.statusMessage = "Internal Server Error";
					resp.body = "{\"success\": false, \"error\": \"Failed to create cube\"}";
					resp.headers["Content-Type"] = "application/json";
					return resp;
				}
				
				scene.addObject(newCube);
				
				// handler->reinitializeSingleObject / handler->notifySceneChanged -- UIHandler débranché
				
				Response resp;
				resp.statusCode = 200;
				resp.statusMessage = "OK";
				resp.body = "{\"success\": true, \"cubeName\": \"" + cubeName + "\", \"id\": " + std::to_string(newCube->getID()) + "}";
				resp.headers["Content-Type"] = "application/json";
				resp.headers["Access-Control-Allow-Origin"] = "*";
				return resp;
			}, "Create a new cube and add it to the scene");
			
			// Route: Select object from hierarchy
			router.post("/api/select-object", [&scene, sdlWindow](const Message& msg) -> Response 
			{
				std::cout << "\n [RoutesManager] /api/select-object called" << std::endl;
				
				// if (handler)
				// {
				// 	std::cout << "[RoutesManager] Calling CallTestFromServer()..." << std::endl;
				// 	// handler->CallTestFromServer();
				// 	std::cout << "[RoutesManager] CallTestFromServer() returned" << std::endl;
				// }
				// else
				// {
				// 	std::cout << "[RoutesManager] ERROR: handler is NULL, cannot call CallTestFromServer()" << std::endl;
				// }
				
				Response resp;
				resp.headers["Content-Type"] = "application/json";
				resp.headers["Access-Control-Allow-Origin"] = "*";
				
				// Parse JSON without exceptions
				auto requestData = nlohmann::json::parse(msg.body, nullptr, false);
				if (requestData.is_discarded()) 
				{
					std::cerr << "[RoutesManager] Error: Invalid JSON in request body" << std::endl;
					resp.statusCode = 400;
					resp.statusMessage = "Bad Request";
					resp.body = "{\"error\": \"Invalid JSON\"}";
					return resp;
				}
				
				// Validate required fields
				if (!requestData.contains("slotIndex") || !requestData.contains("objectName") || !requestData.contains("objectType")) 
				{
					std::cerr << "[RoutesManager] Error: Missing required fields" << std::endl;
					resp.statusCode = 400;
					resp.statusMessage = "Bad Request";
					resp.body = "{\"error\": \"Missing required fields\"}";
					return resp;
				}
				
				int slotIndex = requestData["slotIndex"];
				std::string objectName = requestData["objectName"];
				std::string objectType = requestData["objectType"];
				
				std::cout << "[RoutesManager] Selection request - Slot: " << slotIndex 
						<< " | Object: " << objectName 
						<< " | Type: " << objectType << std::endl;
				
				auto& objects = scene.getObjectsRef();
				
				if (slotIndex < 0 || slotIndex >= static_cast<int>(objects.size())) 
				{
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
				
				for (auto* obj : objects) 
				{
					if (obj) obj->setSelected(false);
				}
				
				bool isCamera = (objectType == "Camera");

				if (!isCamera) 
				{
					selectedObject->setSelected(true);
					if (sdlWindow)
						sdlWindow->onObjectSelectedFromHierarchy(selectedObject);
				} 
				else 
				{
					std::cout << "[RoutesManager] Camera clicked - no gizmo displayed" << std::endl;
				}
				
				resp.statusCode = 200;
				resp.statusMessage = "OK";
				resp.body = "{\"success\": true, \"selected\": \"" + selectedObject->getName() + "\", \"isCamera\": " + (isCamera ? "true" : "false") + "}";
				return resp;
			}, "Select object from hierarchy inspector");
			
			router.post("/api/slot-texture/toggle", [](const Message& msg) -> Response 
			{
				Response resp;
				resp.headers["Content-Type"] = "application/json";
				resp.headers["Access-Control-Allow-Origin"] = "*";
				
				auto requestData = nlohmann::json::parse(msg.body, nullptr, false);
				if (requestData.is_discarded() || !requestData.contains("visible")) 
				{
					resp.statusCode = 400;
					resp.statusMessage = "Bad Request";
					resp.body = "{\"error\": \"Invalid JSON\"}";
					return resp;
				}
				
				bool visible = requestData["visible"];
				
				// handler->enableSlotTextureRendering -- UIHandler débranché
				
				resp.statusCode = 200;
				resp.statusMessage = "OK";
				resp.body = "{\"success\": true, \"visible\": " + std::string(visible ? "true" : "false") + "}";
				return resp;
			}, "Toggle SlotTexture visibility");
			
			router.post("/api/slot-texture/render", [](const Message& msg) -> Response 
			{
				Response resp;
				resp.headers["Content-Type"] = "application/json";
				resp.headers["Access-Control-Allow-Origin"] = "*";
				
				auto requestData = nlohmann::json::parse(msg.body, nullptr, false);
				if (requestData.is_discarded() || !requestData.contains("enable")) 
				{
					resp.statusCode = 400;
					resp.statusMessage = "Bad Request";
					resp.body = "{\"error\": \"Invalid JSON\"}";
					return resp;
				}
				
				bool enable = requestData["enable"];
				
				// handler->CallTestFromServer / handler->enableSlotTextureRendering -- UIHandler débranché
				
				resp.statusCode = 200;
				resp.statusMessage = "OK";
				resp.body = "{\"success\": true, \"enabled\": " + std::string(enable ? "true" : "false") + "}";
				return resp;
			}, "Enable or disable SlotTexture rendering");
			
			std::cout << "[RoutesManager] Object routes registered" << std::endl;
		}

		void RoutesManager::registerIFrameRoutes(RouterSim& router, FrameDataCatcher* frameCatcher, SIMILI::Frontend::UIManager* uiManager)
		{
			// Serve static UI files
			std::cout << "[RoutesManager] Registering static file serving for UI..." << std::endl;
			router.get("/ui/*", [](const Message& msg) -> Response 
			{
				Response resp;
				resp.headers["Access-Control-Allow-Origin"] = "*";
				
				std::string target = std::string(msg.route);
				std::cout << "[RoutesManager] Received request for: " << target << std::endl;
				
				std::string filePath = target.substr(4);
				
				std::string fullPath = "ui/" + filePath;
				std::cout << "[RoutesManager] Attempting to serve: " << fullPath << std::endl;
				
				std::ifstream file(fullPath, std::ios::binary);
				if (!file.is_open())
				{
					std::cout << "[RoutesManager] ERROR: File not found: " << fullPath << std::endl;
					resp.statusCode = 404;
					resp.statusMessage = "Not Found";
					resp.body = "File not found: " + filePath;
					return resp;
				}
				
				std::stringstream buffer;
				buffer << file.rdbuf();
				file.close();
				
				resp.statusCode = 200;
				resp.statusMessage = "OK";
				resp.body = buffer.str();
				
				if (filePath.length() >= 5 && filePath.substr(filePath.length() - 5) == ".html")
				{
					resp.headers["Content-Type"] = "text/html; charset=utf-8";
					resp.headers["Cache-Control"] = "no-cache";
				}
				else if (filePath.length() >= 4 && filePath.substr(filePath.length() - 4) == ".css")
				{
					resp.headers["Content-Type"] = "text/css";
					resp.headers["Cache-Control"] = "no-cache";
				}
				else if (filePath.length() >= 3 && filePath.substr(filePath.length() - 3) == ".js")
				{
					resp.headers["Content-Type"] = "application/javascript";
					resp.headers["Cache-Control"] = "no-cache";
				}
				
				std::cout << "[RoutesManager] Served UI file: " << filePath << " (" << resp.body.size() << " bytes)" << std::endl;
				
				return resp;
			}, "Serve static UI files");
			
			router.post("/api/debug/js-start", [](const Message& msg) -> Response 
			{
				Response resp;
				resp.headers["Access-Control-Allow-Origin"] = "*";
				resp.headers["Content-Type"] = "application/json";
				resp.statusCode = 200;
				resp.statusMessage = "OK";
				resp.body = "{\"success\": true}";
				return resp;
			}, "Debug endpoint for JavaScript startup");

			router.post("/api/debug/iframe-count", [](const Message& msg) -> Response 
			{
				Response resp;
				resp.headers["Access-Control-Allow-Origin"] = "*";
				resp.headers["Content-Type"] = "application/json";
				resp.statusCode = 200;
				resp.statusMessage = "OK";
				resp.body = "{\"success\": true}";
				return resp;
			}, "Debug endpoint for iframe count");

			router.post("/api/debug/panels-found", [](const Message& msg) -> Response 
			{
				Response resp;
				resp.headers["Access-Control-Allow-Origin"] = "*";
				resp.headers["Content-Type"] = "application/json";
				resp.statusCode = 200;
				resp.statusMessage = "OK";
				resp.body = "{\"success\": true}";
				return resp;
			}, "Debug endpoint for panels found");

			router.post("/api/debug/no-panels", [](const Message& msg) -> Response 
			{
				Response resp;
				resp.headers["Access-Control-Allow-Origin"] = "*";
				resp.headers["Content-Type"] = "application/json";
				resp.statusCode = 200;
				resp.statusMessage = "OK";
				resp.body = "{\"success\": true}";
				return resp;
			}, "Debug endpoint for no panels");

			router.post("/api/debug/send-success", [](const Message& msg) -> Response 
			{
				Response resp;
				resp.headers["Access-Control-Allow-Origin"] = "*";
				resp.headers["Content-Type"] = "application/json";
				resp.statusCode = 200;
				resp.statusMessage = "OK";
				resp.body = "{\"success\": true}";
				return resp;
			}, "Debug endpoint for send success");

			router.post("/api/debug/send-error", [](const Message& msg) -> Response 
			{
				Response resp;
				resp.headers["Access-Control-Allow-Origin"] = "*";
				resp.headers["Content-Type"] = "application/json";
				resp.statusCode = 200;
				resp.statusMessage = "OK";
				resp.body = "{\"success\": true}";
				return resp;
			}, "Debug endpoint for send error");

			router.post("/api/debug/init-start", [](const Message& msg) -> Response 
			{
				Response resp;
				resp.headers["Access-Control-Allow-Origin"] = "*";
				resp.headers["Content-Type"] = "application/json";
				resp.statusCode = 200;
				resp.statusMessage = "OK";
				resp.body = "{\"success\": true}";
				return resp;
			}, "Debug endpoint for init start");

			router.post("/api/iframes/update", [frameCatcher](const Message& msg) -> Response {
				std::cout << "[RoutesManager] /api/iframes/update endpoint called" << std::endl;
				std::cout << "[RoutesManager] Request body size: " << msg.body.size() << " bytes" << std::endl;
				
				Response resp;
				resp.headers["Access-Control-Allow-Origin"] = "*";
				resp.headers["Content-Type"] = "application/json";
				
				try
				{
					json requestData = json::parse(msg.body);
					std::cout << "[RoutesManager] JSON parsed successfully" << std::endl;
					
					if (!requestData.contains("iframes") || !requestData["iframes"].is_array())
					{
						std::cout << "[RoutesManager] ERROR: Missing or invalid 'iframes' array" << std::endl;
						resp.statusCode = 400;
						resp.statusMessage = "Bad Request";
						resp.body = "{\"success\": false, \"error\": \"Missing or invalid 'iframes' array\"}";
						return resp;
					}
					
					std::cout << "[RoutesManager] Found " << requestData["iframes"].size() << " iframes in request" << std::endl;
					
					if (!frameCatcher)
					{
						std::cout << "[RoutesManager] ERROR: FrameDataCatcher not available" << std::endl;
						resp.statusCode = 500;
						resp.statusMessage = "Internal Server Error";
						resp.body = "{\"success\": false, \"error\": \"FrameDataCatcher not available\"}";
						return resp;
					}
					
					std::map<std::string, IFrameData> tempIFrameMap;
					
					const int MIN_IFRAME_DIMENSION = 10;
					
					for (const auto& iframe : requestData["iframes"])
					{
						if (iframe.contains("name") && iframe.contains("x") && iframe.contains("y") && 
							iframe.contains("width") && iframe.contains("height"))
						{
							std::string name = iframe["name"];
							int x = iframe["x"];
							int y = iframe["y"];
							int width = iframe["width"];
							int height = iframe["height"];
							int clientX = iframe.contains("clientX") ? iframe["clientX"].get<int>() : x;
							int clientY = iframe.contains("clientY") ? iframe["clientY"].get<int>() : y;
							
							if (width < MIN_IFRAME_DIMENSION || height < MIN_IFRAME_DIMENSION)
							{
								std::cout << "[RoutesManager] Rejecting iframe '" << name << "' with too small dimensions: " 
								          << width << "x" << height << " (min=" << MIN_IFRAME_DIMENSION << ")" << std::endl;
								continue;
							}
							
							IFrameData data;
							data.name = name;
							data.x = x;
							data.y = y;
							data.width = width;
							data.height = height;
							data.clientX = clientX;
							data.clientY = clientY;
							data.layoutIndex = iframe.contains("layoutIndex") ? iframe["layoutIndex"].get<int>() : 0;
							tempIFrameMap[name] = data;
						}
					}
					
					int maxX = 0;
					int maxY = 0;	
					
					for (const auto& pair : tempIFrameMap)
					{
						std::cout << "[RoutesManager] Accepting iframe: " << pair.first 
							<< " (x=" << pair.second.x << ", y=" << pair.second.y 
							<< ", w=" << pair.second.width << ", h=" << pair.second.height << ")" << std::endl;
					}
					
					if (frameCatcher)
					{
						for (const auto& pair : tempIFrameMap)
						{
							frameCatcher->iframe_data_map_[pair.first] = pair.second;
						}
					}					
					
					resp.statusCode = 200;
					resp.statusMessage = "OK";
					resp.body = "{\"success\": true}";
				}
				catch (const std::exception& e)
				{
					std::cout << "[RoutesManager] EXCEPTION: " << e.what() << std::endl;
					resp.statusCode = 500;
					resp.statusMessage = "Internal Server Error";
					resp.body = "{\"success\": false, \"error\": \"" + std::string(e.what()) + "\"}";
				}
				
				return resp;
			}, "Update iframe dimensions");

			std::cout << "[RoutesManager] Registering /api/uipanels/update endpoint..." << std::endl;

			router.post("/api/uipanels/update", [frameCatcher](const Message& msg) -> Response
			{
				Response resp;
				resp.headers["Access-Control-Allow-Origin"] = "*";
				resp.headers["Content-Type"] = "application/json";

				std::cout << "[RoutesManager] /api/uipanels/update endpoint called with " << msg.body.size() << " bytes" << std::endl;

				try
				{
					json requestData = json::parse(msg.body);
					std::cout << "[RoutesManager] JSON parsed successfully" << std::endl;

					if (!requestData.contains("iframes") || !requestData["iframes"].is_array())
					{
						std::cout << "[RoutesManager] Invalid or missing 'iframes' array in request" << std::endl;
						resp.statusCode = 400;
						resp.statusMessage = "Bad Request";
						resp.body = "{\"success\": false, \"error\": \"Missing or invalid 'iframes' array\"}";
						return resp;
					}

					std::cout << "[RoutesManager] Found iframes array with " << requestData["iframes"].size() << " elements" << std::endl;

					if (!frameCatcher)
					{
						resp.statusCode = 500;
						resp.statusMessage = "Internal Server Error";
						resp.body = "{\"success\": false, \"error\": \"FrameDataCatcher not available\"}";
						return resp;
					}

					std::map<std::string, IFrameData> uiPanelIFrames;

					const int MIN_PANEL_DIMENSION = 10;

					for (const auto& iframe : requestData["iframes"])
					{
						if (iframe.contains("name") && iframe.contains("x") && iframe.contains("y") &&
							iframe.contains("width") && iframe.contains("height"))
						{
							std::string name = iframe["name"];
							if (name == "viewport_panel")
							{
								continue;
							}

							int width = iframe["width"];
							int height = iframe["height"];
							
							if (width < MIN_PANEL_DIMENSION || height < MIN_PANEL_DIMENSION)
							{
								std::cout << "[RoutesManager] Rejecting UI panel '" << name << "' with too small dimensions: " 
								          << width << "x" << height << " (min=" << MIN_PANEL_DIMENSION << ")" << std::endl;
								continue;
							}

							IFrameData data;
							data.name = name;
							data.x = iframe["x"];
							data.y = iframe["y"];
							data.width = width;
							data.height = height;
							data.clientX = iframe.contains("clientX") ? iframe["clientX"].get<int>() : data.x;
							data.clientY = iframe.contains("clientY") ? iframe["clientY"].get<int>() : data.y;
							data.layoutIndex = iframe.contains("layoutIndex") ? iframe["layoutIndex"].get<int>() : 0;

							std::cout << "[RoutesManager] Panel '" << name << "' layoutIndex=" << data.layoutIndex << std::endl;

							uiPanelIFrames[name] = data;
							
							if (FrameDataCatcher* catcher = FrameDataCatcher::getInstance())
							{
								catcher->iframe_data_map_[name] = data;
							}
						}
					}

					std::cout << "[RoutesManager] Created " << uiPanelIFrames.size() << " UI panels" << std::endl;


					resp.statusCode = 200;
					resp.statusMessage = "OK";
					resp.body = "{\"success\": true, \"count\": " + std::to_string(uiPanelIFrames.size()) + "}";
				}
				catch (const std::exception& e)
				{
					resp.statusCode = 500;
					resp.statusMessage = "Internal Server Error";
					resp.body = "{\"success\": false, \"error\": \"" + std::string(e.what()) + "\"}";
				}

				return resp;
			}, "Update UI panel iframes");
			
			router.get("/api/iframes/all", [frameCatcher](const Message& msg) -> Response 
			{
				Response resp;
				resp.headers["Access-Control-Allow-Origin"] = "*";
				resp.headers["Content-Type"] = "application/json";
				
				if (!frameCatcher)
				{
					resp.statusCode = 500;
					resp.statusMessage = "Internal Server Error";
					resp.body = "{\"success\": false, \"error\": \"FrameDataCatcher not available\"}";
					return resp;
				}
				
				const auto allIframes = frameCatcher->getAllIFrames();
				
				json responseData = json::array();
				
				for (const auto& pair : allIframes)
				{
					json iframeJson;
					iframeJson["name"] = pair.second.name;
					iframeJson["x"] = pair.second.x;
					iframeJson["y"] = pair.second.y;
					iframeJson["width"] = pair.second.width;
					iframeJson["height"] = pair.second.height;
					iframeJson["clientX"] = pair.second.clientX;
					iframeJson["clientY"] = pair.second.clientY;
					
					responseData.push_back(iframeJson);
				}
				
				resp.statusCode = 200;
				resp.statusMessage = "OK";
				resp.body = responseData.dump();
				
				return resp;
			}, "Get all iframe dimensions");

			router.get("/api/uipanels/all", [uiManager](const Message& msg) -> Response
			{
				Response resp;
				resp.headers["Access-Control-Allow-Origin"] = "*";
				resp.headers["Content-Type"] = "application/json";

				if (!uiManager)
				{
					resp.statusCode = 500;
					resp.statusMessage = "Internal Server Error";
					resp.body = "{\"success\": false, \"error\": \"UIManager not available\"}";
					return resp;
				}

			const auto& allUIPanels = uiManager->getUIPanelIFrames();
			json responseData = json::array();

			for (const auto& pair : allUIPanels)
			{
				json iframeJson;
				iframeJson["name"] = pair.second.name;
				iframeJson["x"] = pair.second.x;
				iframeJson["y"] = pair.second.y;
				iframeJson["width"] = pair.second.width;
				iframeJson["height"] = pair.second.height;
				iframeJson["clientX"] = pair.second.clientX;
				iframeJson["clientY"] = pair.second.clientY;
				responseData.push_back(iframeJson);
			}

			resp.statusCode = 200;
			resp.statusMessage = "OK";
			resp.body = responseData.dump();

			return resp;
		}, "Get all UI panel iframes");
		
		std::cout << "[RoutesManager] IFrame routes registered" << std::endl;

			// Route: receive field bounding rects from object_inspector iframe
			router.post("/api/dataholder/field-positions", [](const Message& msg) -> Response
			{
				Response resp;
				resp.headers["Access-Control-Allow-Origin"] = "*";
				resp.headers["Content-Type"] = "application/json";

				try
				{
					auto body = json::parse(msg.body);
					std::string panelName = body.value("panel", "");
					if (panelName.empty() || !body.contains("fields"))
					{
						resp.statusCode = 400;
						resp.statusMessage = "Bad Request";
						resp.body = "{\"success\":false}";
						return resp;
					}

					DataHolders* dh = DataHolders::getInstance();
					if (dh)
					{
						for (const auto& f : body["fields"])
						{
							std::string field = f.value("field", "");
							int x = f.value("x", 0);
							int y = f.value("y", 0);
							int w = f.value("w", 100);
							int h = f.value("h", 20);
							if (!field.empty())
								dh->setFieldRect(panelName, field, x, y, w, h);
						}
					}

					resp.statusCode = 200;
					resp.statusMessage = "OK";
					resp.body = "{\"success\":true}";
				}
				catch (...)
				{
					resp.statusCode = 400;
					resp.statusMessage = "Bad Request";
					resp.body = "{\"success\":false}";
				}
				return resp;
			}, "Receive field bounding rects for DataHolder overlay positioning");
		}
	}
}