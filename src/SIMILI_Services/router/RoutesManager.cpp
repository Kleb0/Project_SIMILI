#include "RoutesManager.hpp"
#include "../ThirdParty/json.hpp"
#include "../../WorldObjects/Mesh/Mesh.hpp"
#include "../../WorldObjects/Camera/Camera.hpp"
#include "../../Engine/PrimitivesCreation/CreatePrimitive.hpp"
#include "../../SIMILI_Frontend/viewportLogic/HTMLTextureRenderer/TextureEnabler.hpp"
#include <iostream>
#include <sstream>
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
			CefRefPtr<UIHandler>& handler,
			GLFWwindow* glfwWindow)
		{
			std::cout << "[RoutesManager] Initializing all routes..." << std::endl;
			
			registerContextRoutes(router, vkRenderer);
			registerSceneRoutes(router, scene, vkRenderer);
			registerObjectRoutes(router, scene, handler, glfwWindow);
			registerIFrameRoutes(router, handler);
			
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


		void RoutesManager::registerObjectRoutes(RouterSim& router, VKScene& scene, CefRefPtr<UIHandler>& handler, GLFWwindow* glfwWindow)
		{
			router.post("/api/create-cube", [&scene, &handler, glfwWindow](const Message& msg) -> Response 
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
				
				if (handler) 
				{
					handler->reinitializeSingleObject(newCube);
					handler->notifySceneChanged();
				}
				
				Response resp;
				resp.statusCode = 200;
				resp.statusMessage = "OK";
				resp.body = "{\"success\": true, \"cubeName\": \"" + cubeName + "\", \"id\": " + std::to_string(newCube->getID()) + "}";
				resp.headers["Content-Type"] = "application/json";
				resp.headers["Access-Control-Allow-Origin"] = "*";
				return resp;
			}, "Create a new cube and add it to the scene");
			
			// Route: Select object from hierarchy
			router.post("/api/select-object", [&scene, &handler](const Message& msg) -> Response 
			{
				std::cout << "\n [RoutesManager] /api/select-object called - handler is " << (handler ? "VALID" : "NULL") << std::endl;
				
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
				bool shiftKey = requestData.value("shiftKey", false);
				
				std::cout << "[RoutesManager] Selection request - Slot: " << slotIndex 
						<< " | Object: " << objectName 
						<< " | Type: " << objectType 
						<< " | Shift: " << (shiftKey ? "YES" : "NO") << std::endl;
				
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
			
			router.post("/api/slot-texture/toggle", [&handler](const Message& msg) -> Response 
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
				
				if (handler)
				{
					if (!CefCurrentlyOn(TID_UI))
					{
						CefPostTask(TID_UI, new TextureEnablerTask(handler, visible));
					}
					else
					{
						handler->enableSlotTextureRendering(visible);
					}
				}
				
				resp.statusCode = 200;
				resp.statusMessage = "OK";
				resp.body = "{\"success\": true, \"visible\": " + std::string(visible ? "true" : "false") + "}";
				return resp;
			}, "Toggle SlotTexture visibility");
			
			router.post("/api/slot-texture/render", [&handler](const Message& msg) -> Response 
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
				
				if (handler)
				{
					if (enable)
					{
						handler->CallTestFromServer();
					}
					else
					{
						if (!CefCurrentlyOn(TID_UI))
						{
							CefPostTask(TID_UI, new TextureEnablerTask(handler, false));
						}
						else
						{
							handler->enableSlotTextureRendering(false);
						}
					}
				}
				
				resp.statusCode = 200;
				resp.statusMessage = "OK";
				resp.body = "{\"success\": true, \"enabled\": " + std::string(enable ? "true" : "false") + "}";
				return resp;
			}, "Enable or disable SlotTexture rendering");
			
			std::cout << "[RoutesManager] Object routes registered" << std::endl;
		}

		void RoutesManager::registerIFrameRoutes(RouterSim& router, CefRefPtr<UIHandler>& handler)
		{
			// Update iframe dimensions route
			router.post("/api/iframes/update", [&handler](const Message& msg) -> Response 
			{
				Response resp;
				resp.headers["Access-Control-Allow-Origin"] = "*";
				resp.headers["Content-Type"] = "application/json";
				
				try
				{
					json requestData = json::parse(msg.body);
					
					if (!requestData.contains("iframes") || !requestData["iframes"].is_array())
					{
						resp.statusCode = 400;
						resp.statusMessage = "Bad Request";
						resp.body = "{\"success\": false, \"error\": \"Missing or invalid 'iframes' array\"}";
						return resp;
					}
					
					if (!handler)
					{
						resp.statusCode = 500;
						resp.statusMessage = "Internal Server Error";
						resp.body = "{\"success\": false, \"error\": \"Handler not available\"}";
						return resp;
					}
					
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
							
							IFrameData data;
							data.name = name;
							data.x = x;
							data.y = y;
							data.width = width;
							data.height = height;
							data.clientX = clientX;
							data.clientY = clientY;
							handler->iframe_data_map_[name] = data;
						}
					}
					
					handler->captureIFramePositions();
					
					resp.statusCode = 200;
					resp.statusMessage = "OK";
					resp.body = "{\"success\": true}";
				}
				catch (const std::exception& e)
				{
					resp.statusCode = 500;
					resp.statusMessage = "Internal Server Error";
					resp.body = "{\"success\": false, \"error\": \"" + std::string(e.what()) + "\"}";
				}
				
				return resp;
			}, "Update iframe dimensions");

			router.post("/api/uipanels/update", [&handler](const Message& msg) -> Response
			{
				Response resp;
				resp.headers["Access-Control-Allow-Origin"] = "*";
				resp.headers["Content-Type"] = "application/json";

				try
				{
					json requestData = json::parse(msg.body);

					if (!requestData.contains("iframes") || !requestData["iframes"].is_array())
					{
						resp.statusCode = 400;
						resp.statusMessage = "Bad Request";
						resp.body = "{\"success\": false, \"error\": \"Missing or invalid 'iframes' array\"}";
						return resp;
					}

					if (!handler)
					{
						resp.statusCode = 500;
						resp.statusMessage = "Internal Server Error";
						resp.body = "{\"success\": false, \"error\": \"Handler not available\"}";
						return resp;
					}

					std::map<std::string, IFrameData> uiPanelIFrames;
					std::map<std::string, CEF_Drawer::UIPanelFrameData> uiPanelFramesForDrawer;

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

							IFrameData data;
							data.name = name;
							data.x = iframe["x"];
							data.y = iframe["y"];
							data.width = iframe["width"];
							data.height = iframe["height"];
							data.clientX = iframe.contains("clientX") ? iframe["clientX"].get<int>() : data.x;
							data.clientY = iframe.contains("clientY") ? iframe["clientY"].get<int>() : data.y;
							uiPanelIFrames[name] = data;

							CEF_Drawer::UIPanelFrameData panelFrame;
							panelFrame.x = data.x;
							panelFrame.y = data.y;
							panelFrame.width = data.width;
							panelFrame.height = data.height;
							uiPanelFramesForDrawer[name] = panelFrame;
						}
					}

					CEF_Drawer* cefDrawer = handler->getCEFDrawer();
					if (cefDrawer)
					{
						cefDrawer->updateUIPanelFrames(uiPanelFramesForDrawer);
					}

					handler->updateUIPanelIFrames(uiPanelIFrames);
					handler->cacheUIPanelFrameDatas();

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
			
			router.get("/api/iframes/all", [&handler](const Message& msg) -> Response 
			{
				Response resp;
				resp.headers["Access-Control-Allow-Origin"] = "*";
				resp.headers["Content-Type"] = "application/json";
				
				if (!handler)
				{
					resp.statusCode = 500;
					resp.statusMessage = "Internal Server Error";
					resp.body = "{\"success\": false, \"error\": \"Handler not available\"}";
					return resp;
				}
				
				const auto& allIframes = handler->getAllIFrames();
				
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

			router.get("/api/uipanels/all", [&handler](const Message& msg) -> Response
			{
				Response resp;
				resp.headers["Access-Control-Allow-Origin"] = "*";
				resp.headers["Content-Type"] = "application/json";

				if (!handler)
				{
					resp.statusCode = 500;
					resp.statusMessage = "Internal Server Error";
					resp.body = "{\"success\": false, \"error\": \"Handler not available\"}";
					return resp;
				}

				const auto& allUIPanels = handler->getUIPanelIFrames();
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
		}
	}
}