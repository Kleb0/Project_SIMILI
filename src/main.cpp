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
    ThreeDObjectSelector selector;
    Camera mainCamera;
    HierarchyInspector myHierarchy;
    ObjectInspector objectInspector;
    HistoryLogic historyLogic;

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
    myThreeDWindow.setRenderer(renderer);
    mainCamera.setName("MainCamera");
    renderer.setCamera(&mainCamera);

    myThreeDWindow.addThreeDObjectsToScene({ cubeMesh1 });
    myThreeDWindow.addThreeDObjectsToScene({&mainCamera});
    myThreeDWindow.setModelingMode(&myThreeDWindow.normalMode);
    myThreeDWindow.setSimiliSelector(&mySimiliSelector);

    mySimiliSelector.setWindow(&myThreeDWindow);     
    myHierarchy.setTitle("Hierarchy");
    myHierarchy.setContext(&renderer);
    myHierarchy.setThreeDWindow(&myThreeDWindow);
    myHierarchy.setObjectInspector(&objectInspector);
    myThreeDWindow.setHierarchy(&myHierarchy);
    myThreeDWindow.setObjectInspector(&objectInspector);
    objectInspector.setTitle("Object Inspector");
    historyLogic.setTitle("SUPER HISTORY LOGGER");
    historyLogic.setObjectInspector(&objectInspector);
    historyLogic.setOpenGLContext(&renderer);

    gui.add(myInfoWindow);
    gui.add(myThreeDWindow);
    gui.add(objectInspector);
    gui.add(myHierarchy);
    gui.add(historyLogic);

    gui.setThreeDWindow(&myThreeDWindow);
    gui.setObjectInspector(&objectInspector);
    // add(gui, dx12Window);

    gui.run();
    UiCreator::saveCurrentLayoutToDefault();
}
