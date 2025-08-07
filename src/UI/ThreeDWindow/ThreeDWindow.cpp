#define GLM_ENABLE_EXPERIMENTAL

#include <glad/glad.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

#include "UI/ThreeDWindow/ThreeDWindow.hpp"
#include "UI/HierarchyInspectorLogic/HierarchyInspector.hpp"
#include "UI/ObjectInspector.hpp"

#include "WorldObjects/ThreeDObject.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "WorldObjects/Cube.hpp"

#include <SDL3/SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <iostream>
#include <algorithm>
#include <glm/gtx/string_cast.hpp>
// #include <windows.h>

ThreeDWindow::ThreeDWindow() {}

// ------- Constructor, setters and getters ------- //

ThreeDWindow::ThreeDWindow(const std::string &title, const std::string &text)
    : title(title), text(text)
{
}

ThreeDWindow &ThreeDWindow::setRenderer(OpenGLContext &context)
{
    openGLContext = &context;
    return *this;
}

void ThreeDWindow::setModelingMode(ThreeDMode* mode)
{
    std::cout << "[ThreeDWindow] Setting modeling mode to: " << (mode ? mode->getName() : "None") << std::endl;
    if (mode)
    {
        currentMode = mode;
        // ThreeDMode::setMode(std::unique_ptr<ThreeDMode>(mode));
    }
}

void ThreeDWindow::addThreeDObjectsToScene(const std::vector<ThreeDObject *> &objects)
{
    for (auto *object : objects)
    {
        if (object)
        {
            // std::cout << "[ThreeDWindow] Adding object: " << object->getName() << std::endl;
            object->initialize();
            ThreeDObjectsList.push_back(object);
            openGLContext->addThreeDObjectToList(object);

            if(!object->getParent())
            {
                object->setOrigin(openGLContext->worldCenter);
            }

        }
    }
}

void ThreeDWindow::removeThreeDObjectsFromScene(ThreeDObject *object)
{

    std::cout << "[ThreeDWindow] Removing object: " << object->getName() << std::endl;

    for (ThreeDObject *child : object->getChildren())
    {
        std::cout << "[ThreeDWindow] Recursively removing child object: " << child->getName() << std::endl;
        removeThreeDObjectsFromScene(child);
    }

    if (ThreeDObject *parent = object->getParent())
    {
        parent->removeChild(object);
    }

    auto it = std::remove(ThreeDObjectsList.begin(), ThreeDObjectsList.end(), object);
    if (it != ThreeDObjectsList.end())
    {

        openGLContext->removeThreeDobjectFromList(object);
        ThreeDObjectsList.erase(it, ThreeDObjectsList.end());
        std::cout << "[ThreeDWindow] Object removed from ThreeDObjectsList." << std::endl;
        object->destroy();
        // hierarchy->redrawSlotsList();
    }
}

ThreeDObject *ThreeDWindow::getSelectedObject() const
{
    return selector.getSelectedObject();
}

const std::vector<ThreeDObject *> &ThreeDWindow::getObjects() const
{
    return ThreeDObjectsList;
}

// the threeDWindow use external components, we set them in the main script

void ThreeDWindow::setHierarchy(HierarchyInspector *inspector)
{
    hierarchy = inspector;
}

void ThreeDWindow::setObjectInspector(ObjectInspector *inspector)
{
    objectInspector = inspector;
}

// ------- Rendering the ThreeDWindow ------- //

void ThreeDWindow::render()
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_None;
    ImGui::Begin(title.c_str(), nullptr, flags);

    ImGui::Text("%s", text.c_str());

    if (openGLContext)
    {
        ImGui::Text("Attached OpenGL content :");
        onChangeMod();
        threeDRendering();
    }

    ImGui::End();
}

void ThreeDWindow::threeDRendering()
{

    ImGui::BeginChild("OpenGLChildWindow", ImVec2(0, 0), true, ImGuiWindowFlags_None);

    oglChildPos = ImGui::GetCursorScreenPos();
    oglChildSize = ImGui::GetContentRegionAvail();

    int newWidth = static_cast<int>(oglChildSize.x);
    int newHeight = static_cast<int>(oglChildSize.y);

    openGLContext->render();

    ImTextureID textureID = (ImTextureID)(intptr_t)openGLContext->getTexture();
    ImGui::Image(textureID, oglChildSize, ImVec2(0, 1), ImVec2(1, 0));

    ThreeDWorldInteractions();

    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        ImGui::GetCurrentWindow()->Flags |= ImGuiWindowFlags_NoMove;
        handleClick();
    }

    static bool isDragging = false;
    static ImVec2 lastMousePos;

    if (ImGui::IsWindowHovered())
    {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f && openGLContext)
        {
            Camera *cam = dynamic_cast<Camera *>(openGLContext->getCamera());
            if (cam && cam->isSoftwareCamera())
            {
                cam->moveForward(wheel * 0.5f);
            }
        }

        // ------- Camera controls ---------- //
        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
        {
            ImVec2 currentMousePos = ImGui::GetMousePos();

            Camera *cam = dynamic_cast<Camera *>(openGLContext->getCamera());
            if (cam && cam->isSoftwareCamera())
            {
                if (!isDragging)
                {
                    lastMousePos = currentMousePos;
                    isDragging = true;
                }

                float deltaX = currentMousePos.x - lastMousePos.x;
                float deltaY = currentMousePos.y - lastMousePos.y;

                if (deltaX != 0.0f || deltaY != 0.0f)
                {
                    if (!ImGui::GetIO().KeyShift)
                        cam->prepareOrbit();

                    if (ImGui::GetIO().KeyShift)
                        cam->lateralMovement(deltaX, deltaY);
                    else
                        cam->orbitAroundTarget(deltaX, deltaY);
                }

                lastMousePos = currentMousePos;
            }
        }
        else
        {
            isDragging = false;
        }
    }

    renderModelingModes();

    ImGui::EndChild();

}

void ThreeDWindow::renderModelingModes()
{
    ImGui::SetCursorScreenPos(ImVec2(oglChildPos.x + 10, oglChildPos.y + 10));
    ImGui::BeginGroup();

    ImVec2 iconSize = ImVec2(32, 32);
    ImVec2 iconPos0 = ImGui::GetCursorScreenPos();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Icon 0 : Normal Mode
    ImVec2 min0 = iconPos0;
    ImVec2 max0 = ImVec2(min0.x + iconSize.x, min0.y + iconSize.y);

    ImU32 normalBorderColor = (currentMode == &normalMode)
        ? IM_COL32(255, 165, 0, 255)   
        : IM_COL32(255, 255, 255, 255); 

    ImU32 normalTextColor = (currentMode == &normalMode)
        ? IM_COL32(255, 200, 100, 255) 
        : IM_COL32(255, 255, 255, 255);

    draw_list->AddRectFilled(min0, max0, IM_COL32(0, 0, 0, 0));
    draw_list->AddRect(min0, max0, normalBorderColor, 0.0f, 0, 2.0f);
    draw_list->AddText(ImVec2(min0.x + 10, min0.y + 6), normalTextColor, "1");

    // --- End of Icon 0 ---

    ImGui::SetCursorScreenPos(ImVec2(min0.x + iconSize.x + 5, min0.y));

    // === Icon 1 : Vertice Mode ===
    ImGui::SetCursorScreenPos(ImVec2(min0.x + iconSize.x + 5, min0.y));
    ImVec2 iconPos1 = ImGui::GetCursorScreenPos();
    ImVec2 min1 = iconPos1;
    ImVec2 max1 = ImVec2(min1.x + iconSize.x, min1.y + iconSize.y);

    ImU32 verticeBorderColor = (currentMode == &verticeMode)
        ? IM_COL32(255, 165, 0, 255)
        : IM_COL32(255, 255, 255, 255);

    ImU32 verticeTextColor = (currentMode == &verticeMode)
        ? IM_COL32(255, 200, 100, 255)
        : IM_COL32(255, 255, 255, 255);

    draw_list->AddRectFilled(min1, max1, IM_COL32(0, 0, 0, 0));
    draw_list->AddRect(min1, max1, verticeBorderColor, 0.0f, 0, 2.0f);
    draw_list->AddText(ImVec2(min1.x + 10, min1.y + 6), verticeTextColor, "2");

    // --- End of Icon 1 ---

    ImGui::EndGroup();
}

void ThreeDWindow::onChangeMod()
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiKey key1 = ImGuiKey_1;
    ImGuiKey key2 = ImGuiKey_2;

    bool isPressed1 = ImGui::IsKeyDown(key1);
    bool isPressed2 = ImGui::IsKeyDown(key2);

    if (isPressed1  && !lastKeyState_1)
    {
        setModelingMode(&normalMode);
    }


    if (isPressed2 && !lastKeyState_2)
    {
        setModelingMode(&verticeMode);
    }

    lastKeyState_1 = isPressed1;
    lastKeyState_2 = isPressed2;

}

// --------- Object Manipulation ----------- //
void ThreeDWindow::ThreeDWorldInteractions()
{
    if(currentMode == &normalMode)
    {
        MeshTransform::manipulateMesh(
            openGLContext,
            multipleSelectedObjects,
            oglChildPos,
            oglChildSize,
            wasUsingGizmoLastFrame
        );
    }

    if(currentMode == &verticeMode)
    {
        if(lastSelectedVertice)
        {
            VerticeTransform::manipulateVertice(
                openGLContext,
                lastSelectedVertice,
                oglChildPos,
                oglChildSize,
                wasUsingGizmoLastFrame
            );
        }
    }
}

//------- Click Handling ----------- //

void ThreeDWindow::handleClick()
{

    if (selectionLocked)
    {
        selectionLocked = false; // Unlock selection for next click
        return;
    }

    ImVec2 mouse = ImGui::GetMousePos();
    float relativeMouseX = mouse.x - oglChildPos.x;
    float relativeMouseY = mouse.y - oglChildPos.y;
    relativeMouseY = oglChildSize.y - relativeMouseY;

    if (relativeMouseX >= 0 && relativeMouseX <= oglChildSize.x &&
        relativeMouseY >= 0 && relativeMouseY <= oglChildSize.y)
    {
        int windowWidth = static_cast<int>(oglChildSize.x);
        int windowHeight = static_cast<int>(oglChildSize.y);

        view = openGLContext->getViewMatrix();
        proj = openGLContext->getProjectionMatrix();

        if (currentMode == &normalMode)
        {
            std::cout << "[DEBUG] Normal Mode active, click mesh operation done." << std::endl;

            // --------------------------------------

                // ------- selecting an mesh object ------- //
                bool preventSelection = ImGuizmo::IsOver();
                if (!preventSelection)
                {
                    selector.pickUpMesh((int)relativeMouseX, (int)relativeMouseY,
                                        windowWidth, windowHeight, view, proj, ThreeDObjectsList);
                }

                ThreeDObject *selected = selector.getSelectedObject();
                bool shiftPressed = ImGui::GetIO().KeyShift;

                if (selected)
                {
                    // -------- Clicked Object --------
                    if (!shiftPressed)
                    {
                        for (auto *obj : ThreeDObjectsList)
                            obj->setSelected(false);
                        multipleSelectedObjects.clear();
                    }

                    auto it = std::find(multipleSelectedObjects.begin(), multipleSelectedObjects.end(), selected);
                    if (it == multipleSelectedObjects.end())
                    {
                        multipleSelectedObjects.push_back(selected);
                        selected->setSelected(true);
                    }
                    else if (shiftPressed)
                    {
                        multipleSelectedObjects.erase(it);
                        selected->setSelected(false);
                    }

                    if (objectInspector)
                    {
                        if (multipleSelectedObjects.size() > 1)
                        {
                            objectInspector->clearInspectedObject();
                            objectInspector->setMultipleInspectedObjects(multipleSelectedObjects);
                        }
                        else
                        {
                            objectInspector->setInspectedObject(selected);
                        }
                    }

                    if (hierarchy)
                        hierarchy->selectFromThreeDWindow();

                    selector.clearTarget();
                }
                else if (!ImGuizmo::IsUsing() && !wasUsingGizmoLastFrame)
                {
                    // --------- Click without object hit ---------
                    std::cout << "[DEBUG] No object hit — clearing selection." << std::endl;

                    for (auto *obj : ThreeDObjectsList)
                        obj->setSelected(false);

                    multipleSelectedObjects.clear();
                    selector.clearTarget();

                    if (objectInspector)
                    {
                        objectInspector->clearInspectedObject();
                        objectInspector->clearMultipleInspectedObjects();
                    }

                    if (hierarchy)
                    {
                        hierarchy->clearMultipleSelection();
                        hierarchy->unselectObject(hierarchy->getSelectedObject());
                    }

                    wasUsingGizmoLastFrame = false;
                }
                // --------- End of click handling --------
        }

        if (currentMode == &verticeMode)
        {
            if (ImGuizmo::IsUsing())
                return;

            std::cout << "[DEBUG] ThreeDwindow : Vertice Mode active, click vertice operation done." << std::endl;

            Vertice* selectedVertice = selector.pickUpVertice(
                (int)relativeMouseX, (int)relativeMouseY,
                windowWidth, windowHeight, view, proj,
                ThreeDObjectsList
            );

            if (selectedVertice)
            {
                std::cout << "[DEBUG] ThreeDwindow :  Selected vertice: " << selectedVertice->getName() << std::endl;
                selectedVertice->setSelected(true);
                lastSelectedVertice = selectedVertice;
            }
            else
            {
                std::cout << "[DEBUG]  ThreeDwindow :  No vertice hit — clearing vertice selection." << std::endl;

                for (ThreeDObject* obj : ThreeDObjectsList)
                {
                    Cube* cube = dynamic_cast<Cube*>(obj);
                    if (!cube) continue;

                    for (Vertice* vert : cube->getVertices())
                        vert->setSelected(false);
                }

                lastSelectedVertice = nullptr;
            }
        }

      
    }
}

void ThreeDWindow::setMultipleSelectedObjects(const std::list<ThreeDObject*>& objects)
{
    multipleSelectedObjects = objects;
}