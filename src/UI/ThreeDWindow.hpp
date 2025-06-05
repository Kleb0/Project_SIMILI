#pragma once

#include <GLFW/glfw3.h>
#include "UI/GUIWindow.hpp"
#include "Engine/OpenGLContext.hpp"
#include "Engine/ThreeDObjectSelector.hpp"
#include <imgui.h>
#include <string>
#include <vector>

class HierarchyInspector;

class ThreeDObject;

class ObjectInspector;

class ThreeDWindow : public GUIWindow
{
public:
    ThreeDWindow();
    ThreeDWindow(const std::string &title, const std::string &text);

    ImVec2 oglChildPos;
    ImVec2 oglChildSize;
    std::string title = "Hello 3D Window";
    std::string text = "This is a 3D window with OpenGL content.";

    GLFWwindow *glfwWindow = nullptr;

    void render() override;
    void threeDRendering();
    bool wasUsingGizmoLastFrame = false;

    ThreeDWindow &add(OpenGLContext &context);
    ThreeDWindow &add(ThreeDObject &object);
    ThreeDWindow &addObject(ThreeDObject &object);
    const std::vector<ThreeDObject *> &getObjects() const;

    void addThreeDObjectsToScene(const std::vector<ThreeDObject *> &objects);
    void removeThreeDObjectsFromScene(const std::vector<ThreeDObject *> &objects);

    void externalSelect(ThreeDObject *object);
    ThreeDObject *getSelectedObject() const;
    void setHierarchy(HierarchyInspector *inspector);
    void setObjectInspector(ObjectInspector *inspector);

    OpenGLContext *getOpenGLContext() const;

private:
    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 proj = glm::mat4(1.0f);

    OpenGLContext *openGLContext = nullptr;
    ObjectInspector *objectInspector = nullptr;
    HierarchyInspector *hierarchy = nullptr;
    ThreeDObjectSelector selector;
    std::vector<ThreeDObject *> ThreeDObjectsList;

    void handleClick();
    void manipulateThreeDObject();
};