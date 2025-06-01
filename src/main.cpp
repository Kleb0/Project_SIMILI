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
#include "UI/MainSoftwareGUI.hpp"
#include "UI/InfoWindow.hpp"
#include "UI/ThreeDWindow.hpp"
#include "UI/HierarchyInspector.hpp"
#include "WorldObjects/ThreedObject.hpp"
#include "InternalLogic/AssemblerLogic.hpp"
#include "Engine/OpenGLContext.hpp"
#include "Engine/ThreeDObjectSelector.hpp"
#include "WorldObjects/Camera.hpp"
#include "WorldObjects/Cube.hpp"
#include "UI/UiCreator.hpp"
#include "UI/ObjectInspector.hpp"
// #include "UI/DirectX12TestWindow.hpp"

int main(int argc, char **argv)
{
    gExecutableDir = fs::path(argv[0]).parent_path();

    MainSoftwareGUI gui(1280, 720, "Main GUI");
    InfoWindow myInfoWindow;
    ThreeDWindow myThreeDWindow;
    OpenGLContext renderer;
    Cube myCube;
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

    myCube.setName("MonCube");
    mainCamera.setName("MainCamera");

    myThreeDWindow.addThreeDObjectToList(&myCube);
    myThreeDWindow.addThreeDObjectToList(&mainCamera);

    renderer.addThreeDObjectToList(&myCube);
    renderer.addThreeDObjectToList(&mainCamera);

    add(gui, myInfoWindow);
    add(gui, myThreeDWindow);
    myCube.setPosition(glm::vec3(2.5f, 0.5f, 2.5f));
    add(myThreeDWindow, myCube);
    add(myThreeDWindow, mainCamera);
    add(myThreeDWindow, renderer);
    add(renderer, myCube);
    add(renderer, mainCamera);
    renderer.setCamera(&mainCamera);
    myHierarchy.setTitle("Hierarchy");
    myHierarchy.setContext(&renderer);
    myHierarchy.setThreeDWindow(&myThreeDWindow);
    myHierarchy.setObjectInspector(&objectInspector);
    myThreeDWindow.setHierarchy(&myHierarchy);
    myThreeDWindow.setObjectInspector(&objectInspector);
    objectInspector.setTitle("Object Inspector");
    add(gui, objectInspector);
    add(gui, myHierarchy);
    // add(gui, dx12Window);

    gui.run();
    UiCreator::saveCurrentLayoutToDefault();
}
