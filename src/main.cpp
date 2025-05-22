#define SDL_MAIN_HANDLED
#define IMGUI_IMPL_OPENGL_LOADER_GLAD

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <SDL2/SDL.h>
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

int main()
{

    MainSoftwareGUI gui(1280, 720, "Main GUI");
    InfoWindow myInfoWindow;
    ThreeDWindow myThreeDWindow;
    OpenGLContext renderer;
    Cube myCube;
    ThreeDObjectSelector selector;
    Camera mainCamera;
    HierarchyInspector myHierarchy;

    myThreeDWindow.glfwWindow = gui.getWindow();

    myInfoWindow.title = "Hello Window 1";
    myInfoWindow.text = "Bienvenue dans la première fenêtre ! C'est une belle fenêtre !";
    myThreeDWindow.title = "Hello Window 2";
    myThreeDWindow.text = "Bienvenue dans la deuxième fenêtre ! Celle-ci contient un contexte OpenGL !";

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
    myThreeDWindow.setHierarchy(&myHierarchy);
    add(gui, myHierarchy);

    UiCreator::loadLayoutFromFile(UiCreator::getLayoutFilePath("my_custom_ui.ini"));
    gui.run();
    UiCreator::saveCurrentLayoutToDefault();
}
