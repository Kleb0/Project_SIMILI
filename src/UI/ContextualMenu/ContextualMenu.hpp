#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include "Engine/ThreeDScene.hpp"
#include "UI/HierarchyInspectorLogic/HierarchyInspector.hpp"
#include "WorldObjects/Entities/ThreeDObject.hpp"

class ObjectInspector;
class ThreeDScene;
class ContextualMenu
{
public:
    void show();
    void hide();
    void render();

    void setThreeDWindow(ThreeDWindow *window);
    void setHierarchyInspector(HierarchyInspector *inspector);
    void setObjectInspector(ObjectInspector *inspector);
    void setScene(ThreeDScene *scene);
    ThreeDScene* getScene() const { return scene; }

private:
    bool isOpen = false;
    ImVec2 popupPos = ImVec2(0, 0);
    ThreeDWindow *threeDWindow = nullptr;
    HierarchyInspector *hierarchyInspector = nullptr;
    ThreeDObject *selectedObject = nullptr;
    ObjectInspector *objectInspector = nullptr;
    ThreeDObject *pendingDeletion = nullptr;
    ThreeDScene *scene = nullptr;
};