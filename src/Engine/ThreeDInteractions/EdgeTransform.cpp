#include "Engine/ThreeDInteractions/EdgeTransform.hpp"
#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include "Engine/OpenGLContext.hpp"
#include "Engine/Guizmo.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <unordered_set>
#include <list>
#include <cmath>

namespace EdgeTransform
{

    static bool isIdentity(const glm::mat4& m, float eps = 1e-6f)
    {
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                float expected = (i == j) ? 1.0f : 0.0f;
                if (fabs(m[i][j] - expected) > eps) {
                    return false;
                }
            }
        }
        return true;
    }

    glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION op, OpenGLContext* context,
    const std::list<Edge*>& edges,const ImVec2& oglChildPos, const ImVec2& oglChildSize)
    {
        glm::mat4 view = context->getViewMatrix();
        glm::mat4 proj = context->getProjectionMatrix();

        glm::mat4 model = Guizmo::renderGizmoForEdges(edges, op, view, proj, oglChildPos, oglChildSize);
        return model;
    }

    void manipulateEdges(OpenGLContext* context, std::list<Edge*>& selectedEdges,
    const ImVec2& oglChildPos, const ImVec2& oglChildSize, bool& wasUsingGizmoLastFrame)
    {
        if (selectedEdges.empty()) return;

        static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_W)) currentGizmoOperation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) currentGizmoOperation = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_S)) currentGizmoOperation = ImGuizmo::SCALE;

        glm::mat4 view = context->getViewMatrix();
        glm::mat4 proj = context->getProjectionMatrix();

        static glm::mat4 dummyMatrix = glm::mat4(1.0f);
        static glm::mat4 prevDummyMatrix = glm::mat4(1.0f);
        static size_t previousSetHash = 0;

        static glm::mat4 accumDelta = glm::mat4(1.0f);
        static std::vector<Vertice*> vertsSnapshot;

        auto hashSet = [&]() -> size_t {
            size_t h = 1469598103934665603ull;
            for (auto* e : selectedEdges) {
                size_t x = reinterpret_cast<size_t>(e);
                h ^= x; h *= 1099511628211ull;
            }
            h ^= selectedEdges.size();
            return h;
        };

        glm::vec3 center(0.0f);
        int count = 0;
        for (auto* e : selectedEdges)
        {
            if (!e) continue;
            Vertice* a = e->getStart();
            Vertice* b = e->getEnd();
            if (!a || !b) continue;

            ThreeDObject* parent = a->getMeshParent() ? a->getMeshParent() : b->getMeshParent();
            glm::mat4 parentMat = parent ? parent->getModelMatrix() : glm::mat4(1.0f);

            glm::vec3 wa = glm::vec3(parentMat * glm::vec4(a->getLocalPosition(), 1.0f));
            glm::vec3 wb = glm::vec3(parentMat * glm::vec4(b->getLocalPosition(), 1.0f));

            center += 0.5f * (wa + wb);
            ++count;
        }
        if (count > 0) center /= static_cast<float>(count);

        size_t currentHash = hashSet();
        bool usingGizmo = ImGuizmo::IsUsing();
        bool selectionChanged = (currentHash != previousSetHash);

        if (selectionChanged || !usingGizmo) 
        {
            dummyMatrix = glm::translate(glm::mat4(1.0f), center);
            prevDummyMatrix = dummyMatrix;

            if (selectionChanged) 
            {
                accumDelta = glm::mat4(1.0f);
                vertsSnapshot.clear();
            }
            previousSetHash = currentHash;
        }

        Guizmo::renderGizmoForEdges(selectedEdges, currentGizmoOperation, view, proj, oglChildPos, oglChildSize);

        if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                                currentGizmoOperation, ImGuizmo::WORLD, glm::value_ptr(dummyMatrix)))
        {
            glm::mat4 deltaWorld = dummyMatrix * glm::inverse(prevDummyMatrix);

            std::unordered_set<Vertice*> uniqueVerts;
            uniqueVerts.reserve(selectedEdges.size() * 2);
            for (auto* e : selectedEdges) {
                if (!e) continue;
                if (auto* a = e->getStart()) uniqueVerts.insert(a);
                if (auto* b = e->getEnd())   uniqueVerts.insert(b);
            }

            for (auto* v : uniqueVerts)
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

            accumDelta = deltaWorld * accumDelta;

            prevDummyMatrix = dummyMatrix;
            wasUsingGizmoLastFrame = true;
        }
        else
        {
            if (wasUsingGizmoLastFrame) 
            {
                Mesh* parentMesh = nullptr;
                if (!selectedEdges.empty()) 
                {
                    Edge* any = *selectedEdges.begin();
                    if (any) 
                    {
                        Vertice* s = any->getStart();
                        Vertice* e = any->getEnd();
                        ThreeDObject* p = s && s->getMeshParent() ? s->getMeshParent() : (e ? e->getMeshParent() : nullptr);
                        parentMesh = p ? dynamic_cast<Mesh*>(p) : nullptr;
                    }
                }

                vertsSnapshot.clear();
                {
                    std::unordered_set<Vertice*> uniqueVerts;
                    uniqueVerts.reserve(selectedEdges.size() * 2);

                    for (auto* e : selectedEdges) 
                    {
                        if (!e) continue;
                        if (auto* a = e->getStart()) uniqueVerts.insert(a);
                        if (auto* b = e->getEnd()) uniqueVerts.insert(b);
                    }
                    vertsSnapshot.reserve(uniqueVerts.size());
                    for (auto* v : uniqueVerts) if (v) vertsSnapshot.push_back(v);
                }

                if (parentMesh) 
                {
                    if (!isIdentity(accumDelta)) 
                    {
                        if (auto* dna = parentMesh->getMeshDNA()) {
                            dna->trackEdgeModify(accumDelta, vertsSnapshot);
                        }
                    }
                }

                accumDelta = glm::mat4(1.0f);
                vertsSnapshot.clear();
            }
            wasUsingGizmoLastFrame = false;
        }
    }

}