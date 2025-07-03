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

void ThreeDWindow::addThreeDObjectsToScene(const std::vector<ThreeDObject *> &objects)
{
    for (auto *object : objects)
    {
        if (object)
        {
            std::cout << "[ThreeDWindow] Adding object: " << object->getName() << std::endl;
            object->initialize();
            ThreeDObjectsList.push_back(object);
            if (openGLContext)
            {
                std::cout << "[ThreeDWindow] openGLContext is not null, adding object to OpenGL context." << std::endl;
                openGLContext->addThreeDObjectToList(object);
            }
            else
            {
                std::cout << "[ThreeDWindow] openGLContext is null, cannot add object to OpenGL context." << std::endl;
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

void ThreeDWindow::render()
{
    ImGuiWindowFlags flags = ImGuiWindowFlags_None;
    ImGui::Begin(title.c_str(), nullptr, flags);

    ImGui::Text("%s", text.c_str());

    if (openGLContext)
    {
        ImGui::Text("Attached OpenGL content :");
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

    ImGui::EndChild();
}

// --------- Object Manipulation ----------- //
void ThreeDWindow::manipulateThreeDObjects()
{
    if (multipleSelectedObjects.empty())
        return;

    ImGuizmo::BeginFrame();
    ImGuizmo::Enable(true);
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(oglChildPos.x, oglChildPos.y, oglChildSize.x, oglChildSize.y);
    ImGuizmo::SetGizmoSizeClipSpace(0.2f);

    static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;

    if (ImGui::IsKeyPressed(ImGuiKey_W))
        currentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_R))
        currentGizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(ImGuiKey_S))
        currentGizmoOperation = ImGuizmo::SCALE;

    view = openGLContext->getViewMatrix();
    proj = openGLContext->getProjectionMatrix();

    glm::vec3 center(0.0f);
    std::vector<glm::quat> allRotations;
    glm::vec3 averageScale(0.0f);

    for (ThreeDObject *obj : multipleSelectedObjects)
    {
        center += obj->getPosition();
        allRotations.push_back(obj->rotation);
        averageScale += obj->getScale();
    }

    center /= static_cast<float>(multipleSelectedObjects.size());
    averageScale /= static_cast<float>(multipleSelectedObjects.size());

    glm::vec4 cumulative(0.0f);
    for (const glm::quat &q : allRotations)
    {
        glm::quat qn = q;
        if (glm::dot(qn, allRotations[0]) < 0.0f)
            qn = -qn;
        cumulative += glm::vec4(qn.x, qn.y, qn.z, qn.w);
    }
    cumulative = glm::normalize(cumulative);
    glm::quat averageRotation = glm::quat(cumulative.w, cumulative.x, cumulative.y, cumulative.z);

    glm::mat4 dummyMatrix = glm::translate(glm::mat4(1.0f), center);
    dummyMatrix *= glm::toMat4(glm::normalize(averageRotation));
    dummyMatrix = glm::scale(dummyMatrix, averageScale);

    static glm::mat4 prevDummyMatrix = glm::mat4(1.0f);

    bool manipulated = ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(proj),
        currentGizmoOperation,
        ImGuizmo::WORLD,
        glm::value_ptr(dummyMatrix));

    if (manipulated)
    {
        glm::mat4 delta = dummyMatrix * glm::inverse(prevDummyMatrix);

        for (ThreeDObject *obj : multipleSelectedObjects)
        {
            glm::mat4 model = obj->getModelMatrix();
            glm::mat4 transformed = delta * model;
            obj->setModelMatrix(transformed);

            glm::vec3 skew;
            glm::vec4 perspective;
            glm::vec3 pos;
            glm::quat rot;
            glm::vec3 scl;

            if (glm::decompose(transformed, scl, rot, pos, skew, perspective))
            {
                obj->setGlobalPosition(pos);
                obj->setGlobalRotation(glm::degrees(glm::eulerAngles(glm::normalize(rot))));
                obj->setGlobalScale(scl);
            }
        }

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