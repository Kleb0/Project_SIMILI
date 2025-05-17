#pragma once

#include <GLFW/glfw3.h>
#include "UI/GUIWindow.hpp"
#include "UI/SimiliGizmo.hpp"
#include "Engine/OpenGLContext.hpp"
#include "Engine/ThreeDObjectSelector.hpp"
#include <imgui.h>
#include <string>
#include <vector>

class HierarchyInspector;

class ThreeDObject;

class ThreeDWindow : public GUIWindow
{
public:
    SimiliGizmo Similigizmo;
    ImVec2 oglChildPos;
    ImVec2 oglChildSize;
    std::string title = "Hello Window";
    std::string text = "Bienvenue dans la fenêtre 3D !";

    ThreeDWindow();
    ThreeDWindow(const std::string &title, const std::string &text);

    ThreeDWindow &add(OpenGLContext &context);
    ThreeDWindow &add(ThreeDObject &object);
    ThreeDWindow &addObject(ThreeDObject &object);

    GLFWwindow *glfwWindow = nullptr;

    void render() override;
    void threeDRendering();
    bool wasUsingGizmoLastFrame = false;

    // make a list of all the objects in the window
    std::vector<ThreeDObject *> getObjects() const
    {
        return ThreeDObjectsList;
    }

    void addThreeDObjectToList(ThreeDObject *object);

    void setListOfObjects(std::vector<ThreeDObject *> &list)
    {
        ThreeDObjectsList = list;
    }
    void externalSelect(ThreeDObject *object);
    void setHierarchy(HierarchyInspector *inspector);

    ThreeDObject *getSelectedObject() const
    {
        return selector.getSelectedObject();
    }

private:
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 proj = glm::mat4(1.0f);

    OpenGLContext *openGLContext = nullptr;
    HierarchyInspector *hierarchy = nullptr;
    ThreeDObjectSelector selector;
    std::vector<ThreeDObject *> ThreeDObjectsList;

    void handleClick();
    void manipulateThreeDObject();
};