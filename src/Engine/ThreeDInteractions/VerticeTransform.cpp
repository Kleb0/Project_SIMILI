#include "Engine/ThreeDInteractions/VerticeTransform.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "Engine/OpenGLContext.hpp"
#include "Engine/Guizmo.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

namespace VerticeTransform
{

glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION op, OpenGLContext* context,
                            Vertice* vertice, const ImVec2& oglChildPos, const ImVec2& oglChildSize)
{
    glm::mat4 view = context->getViewMatrix();
    glm::mat4 proj = context->getProjectionMatrix();

    glm::mat4 dummyMatrix = glm::translate(glm::mat4(1.0f), vertice->getPosition());
    Guizmo::renderGizmoForVertice(dummyMatrix, op, view, proj, oglChildPos, oglChildSize);

    return dummyMatrix;
}


void manipulateVertice(OpenGLContext* context, Vertice* selectedVertice,
const ImVec2& oglChildPos, const ImVec2& oglChildSize, bool& wasUsingGizmoLastFrame)
{
    if (!selectedVertice) return;

    static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_W)) currentGizmoOperation = ImGuizmo::TRANSLATE;

    glm::mat4 view = context->getViewMatrix();
    glm::mat4 proj = context->getProjectionMatrix();

    // MATRICE persistante entre frames
    static glm::mat4 dummyMatrix = glm::mat4(1.0f);
    static glm::mat4 prevDummyMatrix = glm::mat4(1.0f);
    static Vertice* previousVertice = nullptr;

    glm::vec3 pos = selectedVertice->getPosition();

    if (selectedVertice != previousVertice)
    {
        dummyMatrix = glm::translate(glm::mat4(1.0f), pos);
        prevDummyMatrix = dummyMatrix;
        previousVertice = selectedVertice;
    }

    // Affichage uniquement
    Guizmo::renderGizmoForVertice(dummyMatrix, currentGizmoOperation, view, proj, oglChildPos, oglChildSize);

    if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                              currentGizmoOperation, ImGuizmo::WORLD,
                              glm::value_ptr(dummyMatrix)))
    {
        glm::mat4 delta = dummyMatrix * glm::inverse(prevDummyMatrix);
        glm::vec3 translation = glm::vec3(delta[3]);

        ThreeDObject* parent = selectedVertice->getMeshParent();
        if (!parent) return;

        glm::mat4 parentModelMatrix = parent->getModelMatrix();
        selectedVertice->applyTranslationToLocal(translation, parentModelMatrix);

        prevDummyMatrix = dummyMatrix;
        wasUsingGizmoLastFrame = true;
    }
    else
    {
        wasUsingGizmoLastFrame = false;
    }
}

}