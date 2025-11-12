#include "Engine/ThreeDInteractions/VerticeTransform.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "Engine/ThreeDScene.hpp"
#include "Engine/Guizmo.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <iostream>

#include <unordered_set>
#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Entities/ThreeDObject.hpp"
#include "WorldObjects/Mesh_DNA/Mesh_DNA.hpp"


namespace VerticeTransform
{

    static inline bool isIdentity(const glm::mat4& m) {
        static const glm::mat4 I(1.0f);
        return glm::all(glm::epsilonEqual(glm::vec4(m[0]), glm::vec4(I[0]), 1e-6f)) &&
            glm::all(glm::epsilonEqual(glm::vec4(m[1]), glm::vec4(I[1]), 1e-6f)) &&
            glm::all(glm::epsilonEqual(glm::vec4(m[2]), glm::vec4(I[2]), 1e-6f)) &&
            glm::all(glm::epsilonEqual(glm::vec4(m[3]), glm::vec4(I[3]), 1e-6f));
    }


    glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION op, ThreeDScene* scene, const std::list<Vertice*>& vertices,
    const ImVec2& oglChildPos, const ImVec2& oglChildSize)
    {
            glm::mat4 view = scene->getViewMatrix();
            glm::mat4 proj = scene->getProjectionMatrix();

            glm::mat4 model = Guizmo::renderGizmoForVertices(vertices, op, view, proj, oglChildPos, oglChildSize);
            return model;
    }

    void manipulateVertices(ThreeDScene* scene, const std::list<Vertice*>& selectedVertices,
    const ImVec2& oglChildPos, const ImVec2& oglChildSize, bool& wasUsingGizmoLastFrame,
    const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
    {


        if (selectedVertices.empty()) return;

        static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_W)) currentGizmoOperation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) currentGizmoOperation = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_S)) currentGizmoOperation = ImGuizmo::SCALE;

        glm::mat4 view = viewMatrix;
        glm::mat4 proj = projectionMatrix;

        static glm::mat4 dummyMatrix = glm::mat4(1.0f);
        static glm::mat4 prevDummyMatrix = glm::mat4(1.0f);
        static size_t previousSetHash = 0;

        static glm::mat4 accumDelta = glm::mat4(1.0f);
        static std::vector<Vertice*> vertsSnapshot;
        static bool dragActive = false;


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
        for (auto* v : selectedVertices) 
        {
            if (!v) continue;
            center += v->getPosition();
            ++count;
        }
        if (count > 0) center /= static_cast<float>(count);

        size_t currentHash = hashSet();
        bool usingGizmo = ImGuizmo::IsUsing();
        bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        bool mouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
        bool selectionChanged = (currentHash != previousSetHash);

        if (selectionChanged || !usingGizmo) 
        {
            dummyMatrix = glm::translate(glm::mat4(1.0f), center);
            prevDummyMatrix = dummyMatrix;

            if (selectionChanged) 
            {
                accumDelta = glm::mat4(1.0f);
                vertsSnapshot.clear();
                dragActive = false;
            }
            previousSetHash = currentHash;
        }


        Guizmo::renderGizmoForVertices(selectedVertices, currentGizmoOperation, view, proj, oglChildPos, oglChildSize);

        if (!dragActive && usingGizmo && mouseDown) 
        {
            vertsSnapshot.clear();
            std::unordered_set<Vertice*> uniq;
            uniq.reserve(selectedVertices.size());
            for (auto* v : selectedVertices) if (v) uniq.insert(v);
            vertsSnapshot.reserve(uniq.size());
            for (auto* v : uniq) vertsSnapshot.push_back(v);
            accumDelta = glm::mat4(1.0f);
            prevDummyMatrix = dummyMatrix;
            dragActive = true;
        }

        bool manipulated = ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
        currentGizmoOperation, ImGuizmo::WORLD, glm::value_ptr(dummyMatrix));


        if (usingGizmo && manipulated) 
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

            accumDelta = deltaWorld * accumDelta;
            prevDummyMatrix = dummyMatrix;
        }

        if (dragActive && mouseReleased) 
        {
            Mesh* parentMesh = nullptr;
            if (!selectedVertices.empty()) 
            {
                if (auto* any = selectedVertices.front()) 
                {
                    if (auto* p = any->getMeshParent()) 
                    {
                        parentMesh = dynamic_cast<Mesh*>(p);
                    }
                }
            }
            if (parentMesh && !isIdentity(accumDelta)) 
            {
                if (auto* dna = parentMesh->getMeshDNA()) 
                {
                    std::cout << "Tracking Vertice modification in Mesh DNA." << std::endl;
                    dna->trackVerticeModify(accumDelta, vertsSnapshot);
                }
            }
            accumDelta = glm::mat4(1.0f);
            vertsSnapshot.clear();
            dragActive = false;
        }

        wasUsingGizmoLastFrame = usingGizmo || dragActive;
    }
}