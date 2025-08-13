#include "Engine/ThreeDInteractions/MeshTransform.hpp"
#include "WorldObjects/Entities/ThreeDObject.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "Engine/OpenGLContext.hpp"
#include "UI/ThreeDModes/ThreeDMode.hpp"
#include "UI/ThreeDModes/Vertice_Mode.hpp"
#include "Engine/Guizmo.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <algorithm>

namespace MeshTransform
{

void manipulateChildrens(ThreeDObject* parent, const glm::mat4& delta)
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

void applyGizmoTransformation(const glm::mat4& delta, const std::list<ThreeDObject*>& selectedObjects)
{
    for (ThreeDObject* obj : selectedObjects)
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

glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION op,OpenGLContext* context, const std::list<ThreeDObject*>& selectedObjects, 
const ImVec2& oglChildPos, const ImVec2& oglChildSize)
{
    glm::mat4 view = context->getViewMatrix();
    glm::mat4 proj = context->getProjectionMatrix();

    return Guizmo::renderGizmoForObject(selectedObjects, op, view, proj, oglChildPos, oglChildSize);
}

void manipulateMesh(OpenGLContext* context, const std::list<ThreeDObject*>& selectedObjects,
const ImVec2& oglChildPos, const ImVec2& oglChildSize, bool& wasUsingGizmoLastFrame)
{
    if (selectedObjects.empty()) return;

    static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;

    if (ImGui::IsKeyPressed(ImGuiKey_W)) currentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_R)) currentGizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(ImGuiKey_S)) currentGizmoOperation = ImGuizmo::SCALE;

    glm::mat4 view = context->getViewMatrix();
    glm::mat4 proj = context->getProjectionMatrix();

    glm::mat4 dummyMatrix = Guizmo::renderGizmoForObject(selectedObjects, currentGizmoOperation, view, proj, oglChildPos, oglChildSize);
    static glm::mat4 prevDummyMatrix = glm::mat4(1.0f);

    if (ImGuizmo::Manipulate(
            glm::value_ptr(view),
            glm::value_ptr(proj),
            currentGizmoOperation,
            ImGuizmo::WORLD,
            glm::value_ptr(dummyMatrix)))
    {
        glm::mat4 delta = dummyMatrix * glm::inverse(prevDummyMatrix);
        applyGizmoTransformation(delta, selectedObjects);
        wasUsingGizmoLastFrame = true;
    }

    prevDummyMatrix = dummyMatrix;
    wasUsingGizmoLastFrame = false;
}

}