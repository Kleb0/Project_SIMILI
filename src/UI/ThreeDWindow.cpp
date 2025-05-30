
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
#include <iostream>

void ThreeDWindow::setHierarchy(HierarchyInspector *inspector)
{
    hierarchy = inspector;
}

ThreeDWindow::ThreeDWindow() {}

ThreeDWindow::ThreeDWindow(const std::string &title, const std::string &text)
    : title(title), text(text)
{
}

ThreeDWindow &ThreeDWindow::add(OpenGLContext &context)
{
    openGLContext = &context;
    return *this;
}

ThreeDWindow &ThreeDWindow::addObject(ThreeDObject &object)
{
    ThreeDObjectsList.push_back(&object);
    return *this;
}

void ThreeDWindow::addThreeDObjectToList(ThreeDObject *object)
{
    if (object)
        ThreeDObjectsList.push_back(object);
}

void ThreeDWindow::setObjectInspector(ObjectInspector *inspector)
{
    objectInspector = inspector;
}

ThreeDWindow &ThreeDWindow::add(ThreeDObject &object)
{
    return addObject(object);
}

void ThreeDWindow::handleClick()
{
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

        bool preventSelection = Similigizmo.hasTarget() && ImGuizmo::IsOver();

        if (!preventSelection)
        {
            selector.pickUpTarget((int)relativeMouseX, (int)relativeMouseY, windowWidth, windowHeight, view, proj, ThreeDObjectsList);
        }

        else
        {
            ThreeDObject *target = Similigizmo.getTarget();
            if (target)
            {

                selector.select(target);
                // std::cout << "[SIMILI_GIZMO] Reactivated manually on click." << std::endl;
            }
        }

        ThreeDObject *selected = selector.getSelectedObject();

        if (selected)
        {
            for (auto *obj : ThreeDObjectsList)
                obj->setSelected(false);

            selected->setSelected(true);
            Similigizmo.setTarget(selected);

            if (hierarchy)
                hierarchy->selectFromThreeDWindow();

            if (objectInspector)
                objectInspector->setInspectedObject(selected);
        }
        else
        {

            // std::cout << "[DEBUG] No object selected ! clear Gizmo !" << std::endl;
            if (hierarchy)
            {
                ThreeDObject *previouslySelected = hierarchy->getSelectedObject();
                hierarchy->unselectobject(previouslySelected);
            }

            if (objectInspector)
                objectInspector->clearInspectedObject();

            if (!ImGuizmo::IsUsing() && !wasUsingGizmoLastFrame)
            {

                for (auto *obj : ThreeDObjectsList)
                    obj->setSelected(false);

                Similigizmo.disable();
                selector.clearTarget();
            }
        }
    }
}

void ThreeDWindow::manipulateThreeDObject()
{
    // std::cout << "[DEBUG] UpdateGizmo called!" << std::endl;
    ThreeDObject *selected = selector.getSelectedObject();

    if (!selected)
        return;

    // here is were set the Gizmo when the object is selected
    ImGuizmo::BeginFrame();
    ImGuizmo::Enable(true);
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(oglChildPos.x, oglChildPos.y, oglChildSize.x, oglChildSize.y);
    ImGuizmo::SetGizmoSizeClipSpace(0.2f);

    glm::mat4 model = selected->getModelMatrix();

    static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;

    if (ImGui::IsKeyPressed(ImGuiKey_W))
        currentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_R))
        currentGizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(ImGuiKey_S))
        currentGizmoOperation = ImGuizmo::SCALE;

    bool isManipulating = ImGuizmo::Manipulate(
        glm::value_ptr(view),
        glm::value_ptr(proj),
        currentGizmoOperation,
        ImGuizmo::WORLD,
        glm::value_ptr(model));

    if (ImGuizmo::IsUsing())
    {
        // std::cout << "[DEBUG] Manipulation in progress!" << std::endl;
        // check rotation
        selected->setModelMatrix(model);
        // std::cout << "[DEBUG] New position: " << translation.x << ", " << translation.y << ", " << translation.z << std::endl;
        wasUsingGizmoLastFrame = ImGuizmo::IsUsing();
    }
}

void ThreeDWindow::externalSelect(ThreeDObject *object)
{
    for (auto *obj : ThreeDObjectsList)
        obj->setSelected(false);

    if (object)
    {
        selector.select(object);
        object->setSelected(true);
        Similigizmo.setTarget(object);
        view = openGLContext->getViewMatrix();
        proj = openGLContext->getProjectionMatrix();
    }
    else
    {
        Similigizmo.disable();
        selector.clearTarget();
    }

    if (hierarchy)
        hierarchy->selectFromThreeDWindow();
}

void ThreeDWindow::threeDRendering()
{
    ImGui::BeginChild("OpenGLChildWindow", ImVec2(0, 0), true, ImGuiWindowFlags_None);

    oglChildPos = ImGui::GetCursorScreenPos();
    oglChildSize = ImGui::GetContentRegionAvail();

    int newWidth = static_cast<int>(oglChildSize.x);
    int newHeight = static_cast<int>(oglChildSize.y);

    if (newWidth > 0 && newHeight > 0 &&
        (newWidth != openGLContext->getWidth() || newHeight != openGLContext->getHeight()))
    {
        openGLContext->resize(newWidth, newHeight);
    }

    openGLContext->render();

    ImTextureID textureID = (ImTextureID)(intptr_t)openGLContext->getTexture();

    ImGui::Image(textureID, oglChildSize, ImVec2(0, 1), ImVec2(1, 0));

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

    if (selector.getSelectedObject() != nullptr)
    {
        // the object manipulation is made during the the rendering,
        // and we ensure it is done only if the selector as one or several selected objetcs
        manipulateThreeDObject();
    }

    ImGui::EndChild();
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
