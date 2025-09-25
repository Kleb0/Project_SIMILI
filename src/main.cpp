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

#include "Engine/OpenGLContext.hpp"
#include "Engine/ThreeDScene.hpp"
#include "Engine/ThreeDObjectSelector.hpp"

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
#include "UI/EdgeLoopParameters/EdgeLoopParameters.hpp"
#include "UI/OptionMenu/OptionsMenu.hpp"
#include "Engine/SimiliSelector.hpp"
#include "Engine/ErrorBox.hpp"

// #include "UI/DirectX12TestWindow.hpp"

int main(int argc, char **argv)
{
    gExecutableDir = fs::path(argv[0]).parent_path();

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
    EdgeLoopParameters edgeLoopParameters;
    OptionsMenuContent optionsMenu;


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
    myThreeDWindow.setRenderer(renderer);
    myThreeDWindow.setHierarchy(&myHierarchy);
    myThreeDWindow.setObjectInspector(&objectInspector);
    myThreeDWindow.setThreeDScene(&myThreeDScene);
    
    // Set OpenGL context BEFORE initializing the scene
    myThreeDScene.setOpenGLContext(&renderer);
    myThreeDScene.initizalize(); 

    myThreeDScene.addObject({ cubeMesh1 });
    myThreeDScene.addObject({&mainCamera});

    myThreeDScene.setActiveCamera(&mainCamera);
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
    edgeLoopParameters.setScene(&myThreeDScene);
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
    gui.setEdgeLoopParameters(&edgeLoopParameters);
    gui.setOptionsMenu(&optionsMenu);

    gui.setThreeDWindow(&myThreeDWindow);
    gui.setObjectInspector(&objectInspector);

    // add(gui, dx12Window);

    gui.setScene(&myThreeDScene);

    gui.run();
    UiCreator::saveCurrentLayoutToDefault();
}
