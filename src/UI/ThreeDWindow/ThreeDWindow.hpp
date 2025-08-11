#pragma once

//=== Includes ===//
#include <glad/glad.h>   
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <ImGuizmo.h>
#include "Engine/ThreeDInteractions/MeshTransform.hpp"
#include "Engine/ThreeDInteractions/VerticeTransform.hpp"
#include "Engine/ThreeDInteractions/FaceTransform.hpp"

#include <string>
#include <vector>
#include <list>
#include <set>
#include <iostream>

#include "UI/GUIWindow.hpp"
#include "Engine/OpenGLContext.hpp"
#include "Engine/ThreeDObjectSelector.hpp"
#include "Engine/SimiliSelector.hpp"

#include "UI/HierarchyInspectorLogic/HierarchyInspector.hpp"

#include "UI/ThreeDModes/ThreeDMode.hpp"
#include "UI/ThreeDModes/Normal_Mode.hpp"
#include "UI/ThreeDModes/Vertice_Mode.hpp"
#include "UI/ThreeDModes/Face_Mode.hpp"

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

    HierarchyInspector* getHierarchy() const { return hierarchy; }
    ThreeDObjectSelector& getSelector() { return selector; }

    void setModelingMode(ThreeDMode* mode);
    void render() override;
    void threeDRendering();
    void renderModelingModes();
    void onChangeMod();


    ThreeDWindow &setRenderer(OpenGLContext &context);

    void addThreeDObjectsToScene(const std::vector<ThreeDObject *> &objects);
    void removeThreeDObjectsFromScene(ThreeDObject *object);
    const std::vector<ThreeDObject *> &getObjects() const;

    void setSimiliSelector(SimiliSelector* selector) { similiSelector = selector; }
    SimiliSelector& getSimiliSelector() { return *similiSelector; }


    void lockSelectionOnce() { selectionLocked = true; }

    ThreeDObject *getSelectedObject() const;

    void setHierarchy(HierarchyInspector *inspector);
    void setObjectInspector(ObjectInspector *inspector);

    void setMultipleSelectedObjects(const std::list<ThreeDObject*>& objects);

    ImVec2 oglChildPos;
    ImVec2 oglChildSize;
    std::string title = "Hello 3D Window";
    std::string text = "This is a 3D window with OpenGL content.";
    glm::vec3 centerOfSelection = glm::vec3(0.0f);
    GLFWwindow *glfwWindow = nullptr;
    std::list<ThreeDObject *> multipleSelectedObjects;
    std::list<Vertice *> multipleSelectedVertices;
    std::list<Face *> multipleSelectedFaces;
    bool wasUsingGizmoLastFrame = false;
    bool selectionLocked = false;

    ThreeDMode *currentMode = nullptr;
    Vertice_Mode verticeMode;
    Normal_Mode normalMode;
    Face_Mode faceMode;

    bool lastKeyState_1 = false;
    bool lastKeyState_2 = false;
    bool lastKeyState_3 = false;

private:
    
    SimiliSelector* similiSelector = nullptr;

    void handleClick();
    void toggleMultipleSelection(ThreeDObject *object);

    void ThreeDWorldInteractions();
    

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