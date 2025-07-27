#define GLM_ENABLE_EXPERIMENTAL

#include <glad/glad.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include "UI/ThreeDWindow.hpp"
#include "UI/HierarchyInspector.hpp"
#include "UI/ObjectInspector.hpp"
#include "WorldObjects/ThreeDObject.hpp"
#include <SDL3/SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <iostream>
#include <algorithm>
#include <glm/gtx/string_cast.hpp>
// #include <windows.h>

ThreeDWindow::ThreeDWindow() {}

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

    manipulateThreeDObjects();

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
    draw_list->AddText(ImVec2(min0.x + 10, min0.y + 6), normalTextColor, "0");

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
    draw_list->AddText(ImVec2(min1.x + 10, min1.y + 6), verticeTextColor, "1");

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

void ThreeDWindow::manipulateChildrens(ThreeDObject* parent, const glm::mat4& delta)
{
    for (ThreeDObject* child : parent->getChildren())
    {
        glm::mat4 localModel = child->getModelMatrix();
        glm::mat4 newLocalModel = delta * localModel;
        child->setModelMatrix(newLocalModel);

        glm::vec3 parentWorldOrigin = glm::vec3(parent->getGlobalModelMatrix() * glm::vec4(parent->getOrigin(), 1.0f));
        glm::mat4 childGlobalAfter = child->getGlobalModelMatrix();
        glm::vec3 newLocalOrigin = glm::vec3(glm::inverse(childGlobalAfter) * glm::vec4(parentWorldOrigin, 1.0f));
        child->setOrigin(newLocalOrigin);

        if (child->canHaveChildren)
            manipulateChildrens(child, delta);
    }
}

void ThreeDWindow::applyGizmoTransformation(const glm::mat4& delta)
{
    for (ThreeDObject *obj : multipleSelectedObjects)
    {
        if (obj->getParent())
        {
            glm::mat4 newLocal = delta * obj->getModelMatrix();
            obj->setModelMatrix(newLocal);
        }
        else
        {
            glm::vec3 origin = obj->getOrigin();
            glm::mat4 toOrigin = glm::translate(glm::mat4(1.0f), origin);
            glm::mat4 back = glm::translate(glm::mat4(1.0f), -origin);

            glm::mat4 newGlobal = toOrigin * delta * back * obj->getGlobalModelMatrix();
            obj->setGlobalModelMatrix(newGlobal);
        }

        if (obj->canHaveChildren)
            manipulateChildrens(obj, delta);
    }
}

glm::mat4 ThreeDWindow::prepareGizmoFrame(ImGuizmo::OPERATION op)
{
    ImGuizmo::BeginFrame();
    ImGuizmo::Enable(true);
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(oglChildPos.x, oglChildPos.y, oglChildSize.x, oglChildSize.y);
    ImGuizmo::SetGizmoSizeClipSpace(0.2f);

    view = openGLContext->getViewMatrix();
    proj = openGLContext->getProjectionMatrix();

    glm::vec3 center(0.0f);
    glm::vec3 averageScale(0.0f);
    std::vector<glm::quat> rotations;

    for (ThreeDObject *obj : multipleSelectedObjects)
    {
        center += obj->getPosition();
        rotations.push_back(obj->rotation);
        averageScale += obj->getScale();
    }

    center /= multipleSelectedObjects.size();
    averageScale /= multipleSelectedObjects.size();

    glm::vec4 cumulative(0.0f);
    for (const auto& q : rotations)
    {
        glm::quat aligned = glm::dot(q, rotations[0]) < 0.0f ? -q : q;
        cumulative += glm::vec4(aligned.x, aligned.y, aligned.z, aligned.w);
    }

    cumulative = glm::normalize(cumulative);
    glm::quat avgRotation = glm::quat(cumulative.w, cumulative.x, cumulative.y, cumulative.z);

    glm::mat4 model = glm::translate(glm::mat4(1.0f), center);
    model *= glm::toMat4(glm::normalize(avgRotation));
    model = glm::scale(model, averageScale);

    return model;
}

void ThreeDWindow::manipulateThreeDObjects()
{
    if (multipleSelectedObjects.empty()) return;

    static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;

    if (ImGui::IsKeyPressed(ImGuiKey_W)) currentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_R)) currentGizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(ImGuiKey_S)) currentGizmoOperation = ImGuizmo::SCALE;

    glm::mat4 dummyMatrix = prepareGizmoFrame(currentGizmoOperation);

    static glm::mat4 prevDummyMatrix = glm::mat4(1.0f);

    if (ImGuizmo::Manipulate(
            glm::value_ptr(view),
            glm::value_ptr(proj),
            currentGizmoOperation,
            ImGuizmo::WORLD,
            glm::value_ptr(dummyMatrix)))
    {
        glm::mat4 delta = dummyMatrix * glm::inverse(prevDummyMatrix);
        applyGizmoTransformation(delta);
        wasUsingGizmoLastFrame = true;
    }

    prevDummyMatrix = dummyMatrix;
    wasUsingGizmoLastFrame = false;
}


// --------- Object Selection ----------- //

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
                    selector.pickUpTarget((int)relativeMouseX, (int)relativeMouseY,
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

      
    }
}

void ThreeDWindow::setMultipleSelectedObjects(const std::list<ThreeDObject *> &objects)
{
    multipleSelectedObjects = objects;

    for (ThreeDObject *obj : ThreeDObjectsList)
        obj->setSelected(false);

    for (ThreeDObject *obj : multipleSelectedObjects)
        obj->setSelected(true);

    selector.clearTarget();
    calculatecenterOfSelection(multipleSelectedObjects);
    // We locked the selection to prevent further clicks
    selectionLocked = true;
}

void ThreeDWindow::calculatecenterOfSelection(const std::list<ThreeDObject *> &objects)
{
    if (objects.empty())
    {
        centerOfSelection = glm::vec3(0.0f);
        return;
    }

    glm::vec3 sum(0.0f);

    for (const ThreeDObject *obj : objects)
    {
        sum += obj->getPosition();
    }

    centerOfSelection = sum / static_cast<float>(objects.size());
}

void ThreeDWindow::toggleMultipleSelection(ThreeDObject *object)
{
    auto it = std::find(multipleSelectedObjects.begin(), multipleSelectedObjects.end(), object);
    if (it != multipleSelectedObjects.end())
    {
        multipleSelectedObjects.erase(it);
        object->setSelected(false);
    }
    else
    {
        multipleSelectedObjects.push_back(object);
        object->setSelected(true);
    }
}

void ThreeDWindow::selectMultipleObjects(const std::list<ThreeDObject *> &objects)
{
    multipleSelectedObjects = objects;

    for (ThreeDObject *obj : ThreeDObjectsList)
        obj->setSelected(false);

    for (ThreeDObject *obj : multipleSelectedObjects)
        obj->setSelected(true);
    selector.clearTarget();
}

void ThreeDWindow::externalSelect(ThreeDObject *object)
{
    for (auto *obj : ThreeDObjectsList)
        obj->setSelected(false);

    if (object)
    {
        selector.select(object);
        object->setSelected(true);
        view = openGLContext->getViewMatrix();
        proj = openGLContext->getProjectionMatrix();
    }
    else
    {
        selector.clearTarget();
    }

    if (hierarchy)
        hierarchy->selectFromThreeDWindow();
}