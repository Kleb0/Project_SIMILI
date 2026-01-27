#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include "Engine/ThreeDScene.hpp"
#include "UI/HierarchyInspectorLogic/HierarchyInspector.hpp"
#include "WorldObjects/Entities/ThreeDObject.hpp"
#include "UI/ThreeDWindow/ThreeDWindow.hpp"
#include "UI/ThreeDModes/ThreeDMode.hpp"
#include "UI/ThreeDModes/Face_Mode.hpp"
#include "UI/ThreeDModes/Normal_Mode.hpp"


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
    void setMode(ThreeDMode *mode);
    ThreeDScene* getScene() const { return scene; }

    void setSelectedObject(ThreeDObject* obj);
    const ThreeDObject* getSelectedObject() const { return selectedObject; }


private:
    bool isOpen = false;
    ImVec2 popupPos = ImVec2(0, 0);
    ThreeDWindow *threeDWindow = nullptr;
    HierarchyInspector *hierarchyInspector = nullptr;
    ThreeDObject *selectedObject = nullptr;
    ObjectInspector *objectInspector = nullptr;
    ThreeDObject *pendingDeletion = nullptr;
    ThreeDScene *scene = nullptr;
    ThreeDMode *currentMode = nullptr;
    Face_Mode faceMode;
    Normal_Mode normalMode;


};