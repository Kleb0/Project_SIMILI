#include "Engine/ThreeDInteractions/VerticeTransform.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "Engine/OpenGLContext.hpp"
#include "Engine/Guizmo.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <iostream>

namespace VerticeTransform
{



glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION op, OpenGLContext* context, const std::list<Vertice*>& vertices,
const ImVec2& oglChildPos, const ImVec2& oglChildSize)
{
        glm::mat4 view = context->getViewMatrix();
        glm::mat4 proj = context->getProjectionMatrix();

        glm::mat4 model = Guizmo::renderGizmoForVertices(vertices, op, view, proj, oglChildPos, oglChildSize);
        return model;
}

void manipulateVertices(OpenGLContext* context, const std::list<Vertice*>& selectedVertices,
const ImVec2& oglChildPos, const ImVec2& oglChildSize, bool& wasUsingGizmoLastFrame)
{


    if (selectedVertices.empty()) return;

    static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_W)) currentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_R)) currentGizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(ImGuiKey_S)) currentGizmoOperation = ImGuizmo::SCALE;

    glm::mat4 view = context->getViewMatrix();
    glm::mat4 proj = context->getProjectionMatrix();

    static glm::mat4 dummyMatrix = glm::mat4(1.0f);
    static glm::mat4 prevDummyMatrix = glm::mat4(1.0f);
    static size_t previousSetHash = 0;

    auto hashSet = [&]() -> size_t {
        size_t h = 1469598103934665603ull;
        for (auto* v : selectedVertices) {
            size_t x = reinterpret_cast<size_t>(v);
            h ^= x; h *= 1099511628211ull;
        }
        h ^= selectedVertices.size();
        return h;
    };


    glm::vec3 center(0.0f);
    int count = 0;
    for (auto* v : selectedVertices) {
        if (!v) continue;
        center += v->getPosition();
        ++count;
    }
    if (count > 0) center /= static_cast<float>(count);

    size_t currentHash = hashSet();
    bool usingGizmo = ImGuizmo::IsUsing();

    if (currentHash != previousSetHash || !usingGizmo) {
        dummyMatrix = glm::translate(glm::mat4(1.0f), center);
        prevDummyMatrix = dummyMatrix;
        previousSetHash = currentHash;
    }

    // -------------

    Guizmo::renderGizmoForVertices(selectedVertices, currentGizmoOperation, view, proj, oglChildPos, oglChildSize);
    // ------------

    if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
    currentGizmoOperation, ImGuizmo::WORLD, glm::value_ptr(dummyMatrix)))
    {
        glm::mat4 deltaWorld = dummyMatrix * glm::inverse(prevDummyMatrix);

        for (auto* v : selectedVertices) 
        {
            if (!v) continue;
            ThreeDObject* parent = v->getMeshParent();
            if (!parent) continue;

            const glm::mat4 P  = parent->getModelMatrix();
            const glm::mat4 Pi = glm::inverse(P);

            glm::vec4 L  = glm::vec4(v->getLocalPosition(), 1.0f);
            glm::vec4 W  = P * L;
            glm::vec4 W2 = deltaWorld * W;
            glm::vec4 L2 = Pi * W2;

            v->setLocalPosition(glm::vec3(L2));
            v->setPosition(glm::vec3(W2)); 
        }

        prevDummyMatrix = dummyMatrix;
        wasUsingGizmoLastFrame = true;
    } 
    else
    {
        wasUsingGizmoLastFrame = false;
    }

}




}