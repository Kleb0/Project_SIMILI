#define GLM_ENABLE_EXPERIMENTAL
#define SDL_MAIN_HANDLED
#define IMGUI_IMPL_OPENGL_LOADER_GLAD

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef APIENTRY

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <filesystem>
namespace fs = std::filesystem;
fs::path gExecutableDir;

#include <sstream>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>

#include "Engine/OpenGLContext.hpp"
#include "Engine/ThreeDScene.hpp"
#include "Engine/ThreeDObjectSelector.hpp"
#include "SIMILI_Services/middleware/SimpleHttpServer.hpp"
#include "SIMILI_Services/router/RouterSim.hpp"

#include "WorldObjects/Entities/ThreedObject.hpp"
#include "WorldObjects/Camera/Camera.hpp"

#include "Engine/PrimitivesCreation/CreatePrimitive.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <SDL3/SDL.h>
#include "UI/MainSoftwareGUI.hpp"
#include "UI/HistoryLogic/HistoryLogic.hpp"
#include "UI/InfoWindow.hpp"
#include "UI/ThreeDWindow/ThreeDWindow.hpp"
#include "UI/HierarchyInspectorLogic/HierarchyInspector.hpp"
#include "UI/UIdocking/UiCreator.hpp"
#include "UI/ObjectInspectorLogic/ObjectInspector.hpp"
#include "UI/ThreeDModes/ThreeDMode.hpp"
#include "UI/ThreeDModes/Vertice_Mode.hpp"
#include "UI/ThreeDModes/Normal_Mode.hpp"
#include "UI/ContextualMenu/ContextualMenu.hpp"
#include "UI/OptionMenu/OptionsMenu.hpp"
#include "UI/EdgeLoopControl/EdgeLoopControl.hpp"
#include "Engine/SimiliSelector.hpp"
#include "Engine/ErrorBox.hpp"
#include "Engine/ui_process_manager.hpp"

// #include "UI/DirectX12TestWindow.hpp"

UIProcessManager* g_uiManager = nullptr;

BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType) {
    if (dwCtrlType == CTRL_CLOSE_EVENT || dwCtrlType == CTRL_C_EVENT || 
        dwCtrlType == CTRL_BREAK_EVENT || dwCtrlType == CTRL_LOGOFF_EVENT || 
        dwCtrlType == CTRL_SHUTDOWN_EVENT) {
        
        std::cout << "[ConsoleCtrl] Cleanup signal received, stopping UI process..." << std::endl;
        
        if (g_uiManager) {
            g_uiManager->stop();
        }
        
        return TRUE;
    }
    return FALSE;
}

int main(int argc, char **argv)
{
    gExecutableDir = fs::path(argv[0]).parent_path();

    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    
    std::cout << "[Main] About to start HTTP Server..." << std::endl;
    try {
        SIMILI::Server::SimpleHttpServer::getInstance().start(8080);
        std::cout << "[Main] HTTP Server started successfully" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "[Main] ERROR starting HTTP Server: " << e.what() << std::endl;
        return -1;
    }

    UIProcessManager uiManager;
    g_uiManager = &uiManager;
    
    if (!uiManager.start()) {
        std::cerr << "Error: Failed to launch SIMILI_UI.exe" << std::endl;
    } else {
        std::cout << "SIMILI_UI.exe launched successfully!" << std::endl;
    }

    MainSoftwareGUI gui(1280, 720, "Main GUI");
    SimiliSelector mySimiliSelector;
    InfoWindow myInfoWindow;
    ThreeDWindow myThreeDWindow;
    ThreeDScene myThreeDScene;
    ThreeDObjectSelector selector;
    Camera mainCamera;
    HierarchyInspector myHierarchy;
    ObjectInspector objectInspector;
    HistoryLogic historyLogic;
    ContextualMenu contextualMenu;
    OptionsMenuContent optionsMenu;
    EdgeLoopControl edgeLoopControl;


    Mesh* cubeMesh1 = Primitives::CreateCubeMesh(1.0f,glm::vec3(0.0f, 0.0f, 0.0f), "Cube", true);

    // ------- DirectX 12 has been implemented, so comment it for now as i don't need it actually ------- //
    // If you want to use DirectX 12, uncomment the following lines and make sure to include the necessary headers.

    // DirectX12TestWindow dx12Window;
    // dx12Window.getRenderer()->DetectGPU();

    // dx12Window.title = "DirectX 12 Preview";
    // dx12Window.text = "DirectX 12 is active !";

    // ----------------------------------------- //

    myThreeDWindow.glfwWindow = gui.getWindow();
    myInfoWindow.title = "Project Viewer";
    myThreeDWindow.title = "3D Viewport";

    mainCamera.setName("MainCamera");

    // Create OpenGL context AFTER GLFW is initialized
    OpenGLContext renderer;
    std::cout << "[Main] OpenGL Context ID: " << renderer.getContextID() << std::endl;
    
    myThreeDWindow.setRenderer(renderer);
    myThreeDWindow.setHierarchy(&myHierarchy);
    myThreeDWindow.setObjectInspector(&objectInspector);
    myThreeDWindow.setThreeDScene(&myThreeDScene);
    
    // Set OpenGL context BEFORE initializing the scene
    myThreeDScene.setOpenGLContext(&renderer);
    myThreeDScene.initizalize();
    
    // ===== Register context and scene in ECS for sharing with UI process =====
    std::cout << "[Main] Registering OpenGL context and scene in ECS..." << std::endl;
    auto& registry = SIMILI::Server::SimpleHttpServer::getInstance().getContextRegistry();
    
    // Create an entity for the main process context
    auto mainEntity = registry.createEntity();
    
    // Add components to the entity
    registry.addOpenGLContextComponent(mainEntity, &renderer, renderer.getContextID());
    registry.addSceneComponent(mainEntity, &myThreeDScene, myThreeDScene.getSceneID());
    registry.addMetadataComponent(mainEntity, "SIMILI_Main", "2025-11-06");
    
    std::cout << "[Main] Registered entity " << mainEntity << " with:" << std::endl;
    std::cout << "  - OpenGL Context ID: " << renderer.getContextID() << std::endl;
    std::cout << "  - Scene ID: " << myThreeDScene.getSceneID() << std::endl;
    std::cout << "[Main] Context and scene are now shareable via HTTP server" << std::endl;
    
    auto& router = SIMILI::Server::SimpleHttpServer::getInstance().getRouter();
    router.get("/api/context", [&registry](const SIMILI::Router::Message& msg) -> SIMILI::Router::Response {
        SIMILI::Router::Response res;
        res.statusCode = 200;
        res.headers["Content-Type"] = "application/json";
        
        auto mainEntityOpt = registry.findEntityByProcessName("SIMILI_Main");
        if (!mainEntityOpt.has_value()) {
            res.statusCode = 404;
            res.body = "{\"error\": \"Main process context not found\"}";
            return res;
        }
        
        auto entity = mainEntityOpt.value();
        std::string contextID = "unknown";
        std::string sceneID = "unknown";
        
        auto contextComp = registry.getOpenGLContextComponent(entity);
        if (contextComp.has_value()) {
            contextID = contextComp.value().contextID;
        }
        
        auto sceneComp = registry.getSceneComponent(entity);
        if (sceneComp.has_value()) {
            sceneID = sceneComp.value().sceneID;
        }
        
        res.body = "{\"contextID\": \"" + contextID + "\", \"sceneID\": \"" + sceneID + "\"}";
        return res;
    }, "Expose OpenGL context and scene info");
    std::cout << "[Main] Route /api/context registered in HTTP server" << std::endl;
    
    mainCamera.initialize();
    cubeMesh1->initialize();
    
    glfwPollEvents();
    
    myThreeDScene.addObject(cubeMesh1);
    myThreeDScene.addObject(&mainCamera);
    

    myThreeDScene.setActiveCamera(&mainCamera);
    
    
    myThreeDScene.render();
    myThreeDWindow.setModelingMode(&myThreeDWindow.normalMode);
    myThreeDWindow.setSimiliSelector(&mySimiliSelector);

    mySimiliSelector.setWindow(&myThreeDWindow);      

    historyLogic.setTitle("SUPER HISTORY LOGGER");
    historyLogic.setObjectInspector(&objectInspector);
    historyLogic.setThreeDScene(&myThreeDScene);
    historyLogic.setThreeDWindow(&myThreeDWindow);
    historyLogic.setHierarchyInspector(&myHierarchy);

    auto* sdna = myThreeDScene.getSceneDNA();
    sdna->setSceneRef(&myThreeDScene);

    myThreeDScene.setHierarchyInspector(&myHierarchy);
    myThreeDScene.setThreeDWindow(&myThreeDWindow);
    contextualMenu.setScene(&myThreeDScene);
    contextualMenu.setHierarchyInspector(&myHierarchy);
    contextualMenu.setObjectInspector(&objectInspector);
    contextualMenu.setThreeDWindow(&myThreeDWindow);
    mySimiliSelector.setScene(&myThreeDScene);      

    myHierarchy.setThreeDScene(&myThreeDScene);
    myHierarchy.setThreeDWindow(&myThreeDWindow);
    myHierarchy.setObjectInspector(&objectInspector);

    optionsMenu.setScene(&myThreeDScene);

    gui.add(myInfoWindow);
    gui.add(myThreeDWindow);
    gui.add(objectInspector);
    gui.add(myHierarchy);
    gui.add(historyLogic);
    gui.setContextualMenu(&contextualMenu);
    gui.setOptionsMenu(&optionsMenu);
    gui.setEdgeLoopControl(&edgeLoopControl);
    gui.setThreeDWindow(&myThreeDWindow);
    gui.setObjectInspector(&objectInspector);
    myThreeDWindow.setMainGUI(&gui);
    gui.SetCurrentMode(&myThreeDWindow.normalMode);

    // add(gui, dx12Window);

    gui.setScene(&myThreeDScene);

    gui.run();
    
    UiCreator::saveCurrentLayoutToDefault();
    
    std::cout << "Shutting down SIMILI_UI.exe..." << std::endl;
    uiManager.stop();
    
    return 0;
}
    