#include "Engine/ThreeDInteractions/FaceTransform.hpp"
#include "WorldObjects/Basic/Face.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "WorldObjects/Entities/ThreeDObject.hpp"
#include "WorldObjects/Primitives/Cube.hpp" 
#include "Engine/OpenGLContext.hpp"
#include "Engine/Guizmo.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "Engine/MeshEdit/ExtrudeFace.hpp"

#include <list>
#include <iostream>

namespace FaceTransform
{

glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION op, OpenGLContext* context,
const std::list<Face*>& faces, const ImVec2& oglChildPos, const ImVec2& oglChildSize)
{
    const glm::mat4 view = context->getViewMatrix();
    const glm::mat4 proj = context->getProjectionMatrix();
    return Guizmo::renderGizmoForFaces(faces, op, view, proj, oglChildPos, oglChildSize);
}

void manipulateFaces(OpenGLContext* context, const std::list<Face*>& selectedFaces, const ImVec2& oglChildPos,
const ImVec2& oglChildSize, bool& wasUsingGizmoLastFrame, bool bakeToVertices)
{
    if (selectedFaces.empty()) return;

    static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
    static ImGuizmo::MODE currentGizmoMode = ImGuizmo::WORLD;

    if (ImGui::IsKeyPressed(ImGuiKey_W)) currentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_R)) currentGizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(ImGuiKey_S)) currentGizmoOperation = ImGuizmo::SCALE;

    const glm::mat4 view = context->getViewMatrix();
    const glm::mat4 proj = context->getProjectionMatrix();

    static glm::mat4 dummyMatrix = glm::mat4(1.0f);
    static glm::mat4 prevDummyMatrix = glm::mat4(1.0f);
    static size_t    previousSetHash = 0;


    if (ImGui::IsKeyPressed(ImGuiKey_E)) 
    {
        std::cout << "Extruding faces..." << std::endl;

        std::list<Face*> tempSelection(selectedFaces.begin(), selectedFaces.end());
        const float extrudeDist = 0.2f;
        Face* newCap = FaceTransform::extrudeSelectedFace(tempSelection, extrudeDist);

        if (newCap) {
            const auto& vs = newCap->getVertices();
            ThreeDObject* parent = (vs.empty() || !vs[0]) ? nullptr : vs[0]->getMeshParent();
            const glm::mat4 parentModel = parent ? parent->getModelMatrix() : glm::mat4(1.0f);

            glm::vec3 center(0.0f);
            int count = 0;
            for (auto* v : vs) {
                if (!v) continue;
                const glm::vec3 L = v->getLocalPosition();
                const glm::vec3 W = glm::vec3(parentModel * glm::vec4(L, 1.0f));
                center += W; ++count;
            }
            if (count > 0) center /= float(count);

            currentGizmoOperation = ImGuizmo::TRANSLATE;
            dummyMatrix      = glm::translate(glm::mat4(1.0f), center);
            prevDummyMatrix  = dummyMatrix;
            wasUsingGizmoLastFrame = false;
            return;
        }
    }

    auto hashSet = [&]() -> size_t 
    {
        size_t h = 1469598103934665603ull;
        for (auto* f : selectedFaces) { h ^= reinterpret_cast<size_t>(f); h *= 1099511628211ull; }
        h ^= selectedFaces.size();
        return h;
    };

    auto computeGizmoCenter = [&]() -> glm::vec3 
    {
        glm::vec3 gizmoCenter(0.0f);
        int faceCount = 0;

        for (auto* f : selectedFaces)
        {
            if (!f) continue;
            const auto& verts = f->getVertices();
            if (verts.empty()) continue;

            ThreeDObject* parent = verts[0] ? verts[0]->getMeshParent() : nullptr;
            const glm::mat4 parentModel = parent ? parent->getModelMatrix() : glm::mat4(1.0f);

            glm::vec3 faceCenter(0.0f);
            int vcount = 0;
            for (auto* v : verts) 
            {
                if (!v) continue;
                const glm::vec3 L = v->getLocalPosition();
                const glm::vec3 W = glm::vec3(parentModel * glm::vec4(L, 1.0f));
                faceCenter += W; ++vcount;
            }
            if (vcount > 0) { faceCenter /= float(vcount); gizmoCenter += faceCenter; ++faceCount; }
        }

        if (faceCount > 0) gizmoCenter /= float(faceCount);
        return gizmoCenter;

    };

    const glm::vec3 center = computeGizmoCenter();

    const size_t currentHash = hashSet();
    const bool usingGizmo = ImGuizmo::IsUsing();

    if (currentHash != previousSetHash || !usingGizmo) {
        dummyMatrix = glm::translate(glm::mat4(1.0f), center);
        prevDummyMatrix = dummyMatrix;
        previousSetHash = currentHash;
    }

    Guizmo::renderGizmoForFaces(selectedFaces, currentGizmoOperation, view, proj, oglChildPos, oglChildSize);

    if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
    currentGizmoOperation, ImGuizmo::WORLD, glm::value_ptr(dummyMatrix)))
    {

        const glm::mat4 deltaWorld = dummyMatrix * glm::inverse(prevDummyMatrix);

        for (auto* f : selectedFaces)
        {
            if (!f) continue;

            const auto& verts = f->getVertices();

            ThreeDObject* parent = verts[0]->getMeshParent();
            if (!parent) continue;

            const glm::mat4 parentModel = parent->getModelMatrix();

            f->applyWorldDelta(deltaWorld, parentModel, bakeToVertices);
        }

        prevDummyMatrix = dummyMatrix;
        wasUsingGizmoLastFrame = true;
    }
    else
    {
        wasUsingGizmoLastFrame = false;
    }
}


Face* extrudeSelectedFace(std::list<Face*>& selectedFaces, float distance)
{
    if (selectedFaces.empty()) return nullptr;

    Face* target = selectedFaces.front();
    if (!target) return nullptr;

    const auto& vs = target->getVertices();
    if (vs.empty() || !vs[0]) return nullptr;

    ThreeDObject* owner = vs[0]->getMeshParent();
    if (!owner) return nullptr;

    Cube* cube = dynamic_cast<Cube*>(owner);
    if (!cube) {
        return nullptr;
    }

    auto& verts = const_cast<std::vector<Vertice*>&>(cube->getVertices());
    auto& eds   = const_cast<std::vector<Edge*>&>(cube->getEdges());
    auto& fs    = const_cast<std::vector<Face*>&>(cube->getFaces());

    const size_t beforeCount = fs.size();

    const bool ok = MeshEdit::extrudeQuadFace(cube, verts, eds, fs, target, distance);
    if (!ok) return nullptr;

    if (fs.empty() || fs.size() <= beforeCount) return nullptr;
    Face* newCap = fs.back();
    if (!newCap) return nullptr;

    for (Face* f : selectedFaces) if (f) f->setSelected(false);
    selectedFaces.clear();
    newCap->setSelected(true);
    selectedFaces.push_back(newCap);

    return newCap;
}

}
