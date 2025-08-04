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

#include <glm/gtc/matrix_transform.hpp>
#include <SDL3/SDL.h>
#include "InternalLogic/AssemblerLogic.hpp"
#include "UI/MainSoftwareGUI.hpp"
#include "UI/InfoWindow.hpp"
#include "UI/ThreeDWindow.hpp"
#include "UI/HierarchyInspectorLogic/HierarchyInspector.hpp"
#include "WorldObjects/ThreedObject.hpp"
#include "Engine/OpenGLContext.hpp"
#include "Engine/ThreeDObjectSelector.hpp"
#include "WorldObjects/Camera.hpp"
#include "WorldObjects/Cube.hpp"
#include "UI/UiCreator.hpp"
#include "UI/ObjectInspector.hpp"

#include "UI/ThreeDModes/ThreeDMode.hpp"
#include "UI/ThreeDModes/Vertice_Mode.hpp"
#include "UI/ThreeDModes/Normal_Mode.hpp"

#include "Engine/SimiliSelector.hpp"

// #include "UI/DirectX12TestWindow.hpp"

int main(int argc, char **argv)
{
    gExecutableDir = fs::path(argv[0]).parent_path();

    MainSoftwareGUI gui(1280, 720, "Main GUI");
    SimiliSelector mySimiliSelector;
    InfoWindow myInfoWindow;
    ThreeDWindow myThreeDWindow;
    OpenGLContext renderer;
    Cube myCube;
    Cube myCube2;
    ThreeDObjectSelector selector;
    Camera mainCamera;
    HierarchyInspector myHierarchy;
    ObjectInspector objectInspector;

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
    myThreeDWindow.setRenderer(renderer);

    myCube.setName("SuperGigaCubeTest");
    myCube2.setName("SuperGigaCubeTest2");

    mainCamera.setName("MainCamera");

    myThreeDWindow.addThreeDObjectsToScene({&myCube});
    myThreeDWindow.addThreeDObjectsToScene({&myCube2});
    // myThreeDWindow.removeThreeDObjectsFromScene(&myCube2); // Remove the second cube to test the removal functionality
    myThreeDWindow.addThreeDObjectsToScene({&mainCamera});
    myThreeDWindow.setModelingMode(&myThreeDWindow.normalMode);
    myThreeDWindow.setSimiliSelector(&mySimiliSelector);
    mySimiliSelector.setWindow(&myThreeDWindow); 
    

    renderer.setCamera(&mainCamera);
    myCube.setPosition(glm::vec3(2.5f, 0.5f, 2.5f));
    myCube2.setPosition(glm::vec3(0.0f, 0.0f, 0.0f));

    associate(gui, myInfoWindow);
    associate(gui, myThreeDWindow);

    myHierarchy.setTitle("Hierarchy");
    myHierarchy.setContext(&renderer);
    myHierarchy.setThreeDWindow(&myThreeDWindow);
    myHierarchy.setObjectInspector(&objectInspector);
    myThreeDWindow.setHierarchy(&myHierarchy);
    myThreeDWindow.setObjectInspector(&objectInspector);
    objectInspector.setTitle("Object Inspector");
    associate(gui, objectInspector);
    associate(gui, myHierarchy);
    gui.setThreeDWindow(&myThreeDWindow);
    gui.setObjectInspector(&objectInspector);
    // add(gui, dx12Window);

    gui.run();
    UiCreator::saveCurrentLayoutToDefault();
}
