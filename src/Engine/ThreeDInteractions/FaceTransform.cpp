#include "Engine/ThreeDInteractions/FaceTransform.hpp"
#include "WorldObjects/Basic/Face.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "WorldObjects/Entities/ThreeDObject.hpp"
#include "WorldObjects/Mesh/Mesh.hpp" 
#include "Engine/OpenGLContext.hpp"
#include "Engine/ThreeDScene.hpp"
#include "Engine/Guizmo.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "Engine/MeshEdit/ExtrudeFace.hpp"
#include "Engine/ErrorBox.hpp"

#include <list>
#include <iostream>
 #include <unordered_set>

namespace FaceTransform
{


static inline bool isIdentity(const glm::mat4& m) {
    static const glm::mat4 I(1.0f);
    return glm::all(glm::epsilonEqual(glm::vec4(m[0]), glm::vec4(I[0]), 1e-6f)) &&
    glm::all(glm::epsilonEqual(glm::vec4(m[1]), glm::vec4(I[1]), 1e-6f)) &&
    glm::all(glm::epsilonEqual(glm::vec4(m[2]), glm::vec4(I[2]), 1e-6f)) &&
    glm::all(glm::epsilonEqual(glm::vec4(m[3]), glm::vec4(I[3]), 1e-6f));
}


glm::mat4 prepareGizmoFrame(ImGuizmo::OPERATION op, ThreeDScene* scene,
const std::list<Face*>& faces, const ImVec2& oglChildPos, const ImVec2& oglChildSize)
{
    const glm::mat4 view = scene->getViewMatrix();
    const glm::mat4 proj = scene->getProjectionMatrix();
    return Guizmo::renderGizmoForFaces(faces, op, view, proj, oglChildPos, oglChildSize);
}

void manipulateFaces(ThreeDScene* scene, std::list<Face*>& selectedFaces, const ImVec2& oglChildPos,
const ImVec2& oglChildSize, bool& wasUsingGizmoLastFrame, bool bakeToVertices,
const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix)
{
    if (selectedFaces.empty()) return;

    static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
    static ImGuizmo::MODE currentGizmoMode = ImGuizmo::WORLD;

    if (ImGui::IsKeyPressed(ImGuiKey_W)) currentGizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsKeyPressed(ImGuiKey_R)) currentGizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::IsKeyPressed(ImGuiKey_S)) currentGizmoOperation = ImGuizmo::SCALE;

    const glm::mat4 view = viewMatrix;
    const glm::mat4 proj = projectionMatrix;

    static glm::mat4 accumDelta = glm::mat4(1.0f);
    static std::vector<Vertice*> vertsSnapshot;
    static bool dragActive = false;

    static glm::mat4 dummyMatrix = glm::mat4(1.0f);
    static glm::mat4 prevDummyMatrix = glm::mat4(1.0f);
    static size_t    previousSetHash = 0;


    // ---- Extrusion mechanism call ----
    if (ImGui::IsKeyPressed(ImGuiKey_E)) 
    {
        std::cout << "Extruding faces..." << std::endl;

        
        const float extrudeDist = 0.2f;
        Face* newCap = FaceTransform::extrudeSelectedFace(selectedFaces, extrudeDist);

        if (newCap) 
        {
        const auto& vs = newCap->getVertices();
        ThreeDObject* parent = (vs.empty() || !vs[0]) ? nullptr : vs[0]->getMeshParent();
        const glm::mat4 parentModel = parent ? parent->getModelMatrix() : glm::mat4(1.0f);

        glm::vec3 center(0.0f);
        int count = 0;

        for (auto* v : vs) 
        {
            if (!v) continue;
            const glm::vec3 L = v->getLocalPosition();
            const glm::vec3 W = glm::vec3(parentModel * glm::vec4(L, 1.0f));
            center += W; ++count;
        }
        if (count > 0) center /= float(count);

        currentGizmoOperation = ImGuizmo::TRANSLATE;
        dummyMatrix           = glm::translate(glm::mat4(1.0f), center);
        prevDummyMatrix       = dummyMatrix;
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

    const bool usingGizmo = ImGuizmo::IsUsing();
    const bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    const bool mouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
    const size_t currentHash = hashSet();

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
    
    if (currentHash != previousSetHash || !usingGizmo) 
    {
        dummyMatrix = glm::translate(glm::mat4(1.0f), center);
        prevDummyMatrix = dummyMatrix;
        previousSetHash = currentHash;
        accumDelta = glm::mat4(1.0f);
        vertsSnapshot.clear();
        dragActive = false;
    }

    

    Guizmo::renderGizmoForFaces(selectedFaces, currentGizmoOperation, view, proj, oglChildPos, oglChildSize);

    if (!dragActive && usingGizmo && mouseDown)
    {
        std::unordered_set<Vertice*> uniq;
        for (auto* f : selectedFaces) {
            if (!f) continue;
            for (auto* v : f->getVertices()) if (v) uniq.insert(v);
        }
        vertsSnapshot.assign(uniq.begin(), uniq.end());
        accumDelta = glm::mat4(1.0f);
        prevDummyMatrix = dummyMatrix;
        dragActive = true;
    }


    if (ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
    currentGizmoOperation, ImGuizmo::WORLD, glm::value_ptr(dummyMatrix)))
    {
        const glm::mat4 deltaWorld = dummyMatrix * glm::inverse(prevDummyMatrix);

        for (auto* f : selectedFaces)
        {
            if (!f) continue;
            const auto& verts = f->getVertices();
            if (verts.empty() || !verts[0]) continue;
            ThreeDObject* parent = verts[0]->getMeshParent();
            if (!parent) continue;

            const glm::mat4 parentModel = parent->getModelMatrix();
            f->applyWorldDelta(deltaWorld, parentModel, bakeToVertices);
        }

        accumDelta      = deltaWorld * accumDelta;
        prevDummyMatrix = dummyMatrix;
        wasUsingGizmoLastFrame = true;
    }
    else
    {
        wasUsingGizmoLastFrame = usingGizmo || dragActive;
    }

    if (dragActive && mouseReleased)
    {
        Mesh* parentMesh = nullptr;

        for (auto* f : selectedFaces)
        {
            if (!f) continue;
            const auto& vs = f->getVertices();
            if (!vs.empty() && vs[0]) {
                if (auto* p = vs[0]->getMeshParent()) 
                {
                    parentMesh = dynamic_cast<Mesh*>(p);
                    if (parentMesh) break;
                }
            }
        }

        if (parentMesh && !isIdentity(accumDelta))
        {
            if (auto* dna = parentMesh->getMeshDNA())
            {
                dna->trackFaceModify(accumDelta, vertsSnapshot);
            }
        }

        accumDelta = glm::mat4(1.0f);
        vertsSnapshot.clear();
        dragActive = false;
    } 
}


Face* extrudeSelectedFace(std::list<Face*>& selectedFaces, float distance)
{
    if (selectedFaces.empty()) return nullptr;

    Face* target = selectedFaces.front();
    if (!target) return nullptr;

    target->setSelected(false);

    const auto& vs = target->getVertices();
    if (vs.empty() || !vs[0]) return nullptr;

    ThreeDObject* owner = vs[0]->getMeshParent();
    if (!owner) return nullptr;

    Mesh* mesh = dynamic_cast<Mesh*>(owner);
    if (!mesh) return nullptr;

    auto& verts = const_cast<std::vector<Vertice*>&>(mesh->getVertices());
    auto& eds = const_cast<std::vector<Edge*>&>(mesh->getEdges());
    auto& fs = const_cast<std::vector<Face*>&>(mesh->getFaces());

    const size_t beforeCount = fs.size();

    // showErrorBox("FaceTransform.cpp L257 : record creation");
    MeshEdit::ExtrudeResult res{};

    const bool ok = MeshEdit::extrudeQuadFace(mesh, verts, eds, fs, target, distance, &res);

    if (!ok) return nullptr;

        if (auto* dna = mesh->getMeshDNA()) 
        {
            ExtrudeRecord rec{};

            for (int i=0;i<4;++i)
            {
                rec.newVerts[i] = res.newVerts[i];
                rec.capEdges[i] = res.capEdges[i];
                rec.upEdges[i]  = res.upEdges[i];
                rec.sideFaces[i]= res.sideFaces[i];
                rec.oldVerts[i] = res.oldVerts[i];
                rec.oldEdges[i] = res.oldEdges[i];
            }
            rec.capFace = res.capFace;
            rec.distance = res.distance;
            dna->trackExtrude(rec);
        }
    // ------

    if (fs.empty() || fs.size() <= beforeCount) return nullptr;
    Face* newCap = fs.back();
    if (!newCap) return nullptr;

    selectedFaces.clear();

    for (Face* f : selectedFaces) if (f) f->setSelected(false);
    selectedFaces.clear();
    newCap->setSelected(true);
    selectedFaces.push_back(newCap);

    return newCap;
}

}
