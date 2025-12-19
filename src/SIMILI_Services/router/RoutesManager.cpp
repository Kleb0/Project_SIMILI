#include "RoutesManager.hpp"
#include "../ThirdParty/json.hpp"
#include "../../WorldObjects/Mesh/Mesh.hpp"
#include "../../WorldObjects/Camera/Camera.hpp"
#include "../../Engine/PrimitivesCreation/CreatePrimitive.hpp"
#include "../../SIMILI_Frontend/UI_Engine/viewportLogic/Keymanagement/IFrameSizeStocker.hpp"
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
    OpenGLContext& renderer,
    ThreeDScene& scene,
    CefRefPtr<UIHandler>& handler,
    GLFWwindow* glfwWindow)
{
    std::cout << "[RoutesManager] Initializing all routes..." << std::endl;
    
    registerContextRoutes(router, renderer);
    registerSceneRoutes(router, scene, renderer);
    registerObjectRoutes(router, scene, handler, glfwWindow);
    registerIFrameRoutes(router);
    
    std::cout << "[RoutesManager] All routes registered successfully" << std::endl;
}


void RoutesManager::registerContextRoutes(RouterSim& router, OpenGLContext& renderer)
{
    router.get("/api/context", [&renderer](const Message& msg) -> Response 
    {
        Response resp;
        resp.statusCode = 200;
        resp.statusMessage = "OK";
        resp.body = "{\"contextId\": \"" + renderer.getContextID() + "\"}";
        resp.headers["Content-Type"] = "application/json";
        return resp;
    }, "Get OpenGL context ID");
    
    std::cout << "[RoutesManager] Context routes registered" << std::endl;
}


void RoutesManager::registerSceneRoutes(RouterSim& router, ThreeDScene& scene, OpenGLContext& renderer)
{
    // Route: Get scene objects
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
    
    // Route: Get scene info (Scene ID + Context ID)
    router.get("/api/scene-info", [&scene, &renderer](const Message& msg) -> Response 
    {
        Response resp;
        resp.statusCode = 200;
        resp.statusMessage = "OK";
        
        std::ostringstream json;
        json << "{"
             << "\"sceneID\":\"" << scene.getSceneID() << "\","
             << "\"contextID\":\"" << renderer.getContextID() << "\""
             << "}";
        
        resp.body = json.str();
        resp.headers["Content-Type"] = "application/json";
        resp.headers["Access-Control-Allow-Origin"] = "*";
        return resp;
    }, "Get scene and context IDs");
    
    std::cout << "[RoutesManager] Scene routes registered" << std::endl;
}


void RoutesManager::registerObjectRoutes(RouterSim& router, ThreeDScene& scene, CefRefPtr<UIHandler>& handler, GLFWwindow* glfwWindow)
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
                
                std::cout << "[RoutesManager] " << selectedList.size() << " object(s) selected, gizmo render forced" << std::endl;
            }
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
    
    std::cout << "[RoutesManager] Object routes registered" << std::endl;
}

void RoutesManager::registerIFrameRoutes(RouterSim& router)
{
    router.post("/api/iframes/update", [](const Message& msg) -> Response 
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
            
            auto& stocker = IFrameSizeStocker::getInstance();
            
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
                    
                    stocker.updateIFrameData(name, x, y, width, height);
                }
            }
            
            resp.statusCode = 200;
            resp.statusMessage = "OK";
            resp.body = "{\"success\": true}";
        }
        catch (const json::exception& e)
        {
            std::cerr << "[RoutesManager] JSON parse error: " << e.what() << std::endl;
            resp.statusCode = 400;
            resp.statusMessage = "Bad Request";
            resp.body = "{\"success\": false, \"error\": \"Invalid JSON\"}";
        }
        
        return resp;
    }, "Update iframe dimensions");
    
    router.get("/api/iframes/all", [](const Message& msg) -> Response 
    {
        Response resp;
        resp.headers["Access-Control-Allow-Origin"] = "*";
        resp.headers["Content-Type"] = "application/json";
        
        auto& stocker = IFrameSizeStocker::getInstance();
        auto allIframes = stocker.getAllIFrames();
        
        json responseData = json::array();
        
        for (const auto& pair : allIframes)
        {
            json iframeJson;
            iframeJson["name"] = pair.second.name;
            iframeJson["x"] = pair.second.x;
            iframeJson["y"] = pair.second.y;
            iframeJson["width"] = pair.second.width;
            iframeJson["height"] = pair.second.height;
            responseData.push_back(iframeJson);
        }
        
        resp.statusCode = 200;
        resp.statusMessage = "OK";
        resp.body = responseData.dump();
        return resp;
    }, "Get all iframe dimensions");
    
    std::cout << "[RoutesManager] IFrame routes registered" << std::endl;
}

} // namespace Router
} // namespace SIMILI
