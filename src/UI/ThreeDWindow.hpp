#pragma once

//=== Includes ===//
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include "Engine/Guizmo.hpp"

#include <string>
#include <vector>
#include <list>
#include <set>
#include <iostream>

#include "UI/GUIWindow.hpp"
#include "Engine/OpenGLContext.hpp"
#include "Engine/ThreeDObjectSelector.hpp"

#include "UI/ThreeDModes/ThreeDMode.hpp"
#include "UI/ThreeDModes/Normal_Mode.hpp"
#include "UI/ThreeDModes/Vertice_Mode.hpp"

//=== Forward declarations ===//
class HierarchyInspector;
class ObjectInspector;
class ThreeDObject;
class Vertice;

//=== Class ===//
class ThreeDWindow : public GUIWindow
{
public:
    ThreeDWindow();
    ThreeDWindow(const std::string &title, const std::string &text);


    void setModelingMode(ThreeDMode* mode);
    void render() override;
    void threeDRendering();
    void renderModelingModes();
    void onChangeMod();


    ThreeDWindow &setRenderer(OpenGLContext &context);

    void addThreeDObjectsToScene(const std::vector<ThreeDObject *> &objects);
    void removeThreeDObjectsFromScene(ThreeDObject *object);
    const std::vector<ThreeDObject *> &getObjects() const;

    void externalSelect(ThreeDObject *object);
    ThreeDObject *getSelectedObject() const;
    void selectMultipleObjects(const std::list<ThreeDObject *> &objects);
    void setMultipleSelectedObjects(const std::list<ThreeDObject *> &objects);
    void calculatecenterOfSelection(const std::list<ThreeDObject *> &objects);

    void setHierarchy(HierarchyInspector *inspector);
    void setObjectInspector(ObjectInspector *inspector);

    ImVec2 oglChildPos;
    ImVec2 oglChildSize;
    std::string title = "Hello 3D Window";
    std::string text = "This is a 3D window with OpenGL content.";
    glm::vec3 centerOfSelection = glm::vec3(0.0f);
    GLFWwindow *glfwWindow = nullptr;
    std::list<ThreeDObject *> multipleSelectedObjects;
    bool wasUsingGizmoLastFrame = false;
    bool selectionLocked = false;

    ThreeDMode *currentMode = nullptr;
    Vertice_Mode verticeMode;
    Normal_Mode normalMode;

    bool lastKeyState_1 = false;
    bool lastKeyState_2 = false;

private:
    void handleClick();
    void toggleMultipleSelection(ThreeDObject *object);
    glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION operation);
    void applyGizmoTransformation(const glm::mat4 &delta);
    void manipulateThreeDObjects();
    
    void manipulateChildrens(ThreeDObject *parent, const glm::mat4 &delta);

    glm::mat4 view = glm::mat4(1.0f);
    glm::mat4 proj = glm::mat4(1.0f);
    OpenGLContext *openGLContext = nullptr;
    ObjectInspector *objectInspector = nullptr;
    HierarchyInspector *hierarchy = nullptr;
    ThreeDObjectSelector selector;
    std::vector<ThreeDObject *> ThreeDObjectsList;
    std::set<ThreeDObject *> lastSelection;
    Vertice* lastSelectedVertice = nullptr;
    ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;


};