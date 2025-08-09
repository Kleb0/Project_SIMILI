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
        glm::mat4 delta = dummyMatrix * glm::inverse(prevDummyMatrix);
        glm::vec3 translation = glm::vec3(delta[3]);

        for (auto* v : selectedVertices) {
            if (!v) continue;
            ThreeDObject* parent = v->getMeshParent();
            if (!parent) continue;
            glm::mat4 parentModelMatrix = parent->getModelMatrix();
            v->applyTranslationToLocal(translation, parentModelMatrix);

            // ----- We get the new world position from the parent model matrix
            glm::vec3 newWorld = glm::vec3(parentModelMatrix * glm::vec4(v->getLocalPosition(), 1.0f));

            // ----- We set the local position to the global position for each vertice
            v->setPosition(newWorld);
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