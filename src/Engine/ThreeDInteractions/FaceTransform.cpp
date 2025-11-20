#include "Engine/ThreeDInteractions/FaceTransform.hpp"
#include "WorldObjects/Basic/Face.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "WorldObjects/Entities/ThreeDObject.hpp"
#include "WorldObjects/Mesh/Mesh.hpp" 
#include "Engine/OpenGLContext.hpp"
#include "Engine/ThreeDScene.hpp"
#include "Engine/Guizmo.hpp"
#include "SIMILI_Frontend/UI_Engine/viewportLogic/Keymanagement/KeyManager.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include "Engine/MeshEdit/ExtrudeFace.hpp"
#include "Engine/ErrorBox.hpp"

#include <list>
#include <iostream>
#include <unordered_set>
#include <unordered_map>

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
    static ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
    static ImGuizmo::MODE currentGizmoMode = ImGuizmo::WORLD;
    
    // CRITICAL: Check key presses BEFORE checking if faces are selected
    // This allows mode switching even when nothing is selected
    auto& keyManager = SIMILI::Input::KeyManager::getInstance();
    auto* inputSystem = keyManager.getInputSystem();
    
    if (inputSystem)
    {
        inputSystem->pollKeyStates();
        
        const auto* wKeyState = inputSystem->getKeyState('W');
        const auto* rKeyState = inputSystem->getKeyState('R');
        const auto* sKeyState = inputSystem->getKeyState('S');
        
        if (wKeyState && wKeyState->isFirstPress)
        {
            currentGizmoOperation = ImGuizmo::TRANSLATE;
        }
        if (rKeyState && rKeyState->isFirstPress)
        {
            currentGizmoOperation = ImGuizmo::ROTATE;
        }
        if (sKeyState && sKeyState->isFirstPress)
        {
            currentGizmoOperation = ImGuizmo::SCALE;
        }
    }
    
    // Early return if no faces selected - mode switching still works above
    if (selectedFaces.empty()) return;

    const glm::mat4 view = viewMatrix;
    const glm::mat4 proj = projectionMatrix;

    static glm::mat4 dummyMatrix = glm::mat4(1.0f);
    static glm::mat4 startMatrix = glm::mat4(1.0f);
    static glm::mat4 prevMatrix = glm::mat4(1.0f);
    static size_t previousSetHash = 0;
    static bool gizmoActive = false;
    static glm::vec3 lockedCenter = glm::vec3(0.0f);  // CRITICAL: Lock center during drag
    static ThreeDObject* lockedParent = nullptr;

    static std::unordered_map<Vertice*, glm::vec3> initialLocalPositions;
    static glm::mat4 totalAccumDelta = glm::mat4(1.0f);


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
        prevMatrix            = dummyMatrix;
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

    size_t currentHash = hashSet();
    bool usingGizmo = ImGuizmo::IsUsing();
    bool selectionChanged = (currentHash != previousSetHash);

    // Calculate center of selected faces in world space (only when NOT dragging)
    glm::vec3 center = lockedCenter;
    ThreeDObject* firstParent = lockedParent;
    
    if (!gizmoActive)
    {
        glm::vec3 gizmoCenter(0.0f);
        int faceCount = 0;
        firstParent = nullptr;

        for (auto* f : selectedFaces)
        {
            if (!f) continue;
            const auto& verts = f->getVertices();
            if (verts.empty()) continue;

            if (!firstParent && verts[0]) {
                firstParent = verts[0]->getMeshParent();
            }

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
        center = gizmoCenter;
    }
    
    if (selectionChanged || (!usingGizmo && !gizmoActive)) 
    {
        dummyMatrix = glm::translate(glm::mat4(1.0f), center);
        startMatrix = dummyMatrix;
        prevMatrix = dummyMatrix;
        gizmoActive = false;
        lockedCenter = center;
        lockedParent = firstParent;
        initialLocalPositions.clear();
        totalAccumDelta = glm::mat4(1.0f);
        
        if (selectionChanged)
        {
            previousSetHash = currentHash;
        }
    }

    // CRITICAL: Always render the gizmo, not just when inactive
    glm::mat4 gizmoMatrix = Guizmo::renderGizmoForFaces(selectedFaces, currentGizmoOperation, view, proj, oglChildPos, oglChildSize);
    
    // Only update dummyMatrix if we're not actively dragging
    if (!gizmoActive)
    {
        dummyMatrix = gizmoMatrix;
    }

    // Start of drag: capture initial vertex positions and LOCK the center
    if (usingGizmo && !gizmoActive)
    {
        gizmoActive = true;
        lockedCenter = center;
        lockedParent = firstParent;
        startMatrix = dummyMatrix;
        prevMatrix = dummyMatrix;
        initialLocalPositions.clear();
        totalAccumDelta = glm::mat4(1.0f);
        
        std::unordered_set<Vertice*> uniqueVerts;
        for (auto* f : selectedFaces) {
            if (!f) continue;
            for (auto* v : f->getVertices()) if (v) uniqueVerts.insert(v);
        }
        
        for (auto* v : uniqueVerts)
        {
            if (v) {
                initialLocalPositions[v] = v->getLocalPosition();
            }
        }
    }

    // Manipulate the gizmo
    const bool Manipulated = ImGuizmo::Manipulate(
        glm::value_ptr(view), 
        glm::value_ptr(proj),
        currentGizmoOperation, 
        ImGuizmo::WORLD, 
        glm::value_ptr(dummyMatrix)
    );

    // During drag: apply transformation from INITIAL positions
    if (usingGizmo && Manipulated && lockedParent)
    {
        // Calculate TOTAL delta from start of drag
        glm::mat4 totalDelta = dummyMatrix * glm::inverse(startMatrix);
        
        // Check if delta is not identity
        if (!isIdentity(totalDelta))
        {
            glm::mat4 parentMat = lockedParent->getModelMatrix();
            glm::mat4 parentInv = glm::inverse(parentMat);
            
            // Apply total transformation from initial positions
            for (auto& pair : initialLocalPositions)
            {
                Vertice* v = pair.first;
                glm::vec3 initialLocalPos = pair.second;
                
                if (!v) continue;
                
                // Transform initial local position to world space
                glm::vec3 initialWorldPos = glm::vec3(parentMat * glm::vec4(initialLocalPos, 1.0f));
                
                // Apply total transformation in world space
                glm::vec4 transformedWorld = totalDelta * glm::vec4(initialWorldPos, 1.0f);
                
                // Transform back to local space
                glm::vec3 newLocalPos = glm::vec3(parentInv * transformedWorld);
                
                v->setLocalPosition(newLocalPos);
                v->setPosition(glm::vec3(transformedWorld));
            }
            
            // Store for DNA tracking
            totalAccumDelta = totalDelta;
        }
        
        prevMatrix = dummyMatrix;
    }

    // End of drag: track changes in DNA
    if (!usingGizmo && gizmoActive)
    {
        Mesh* parentMesh = nullptr;
        std::vector<Vertice*> vertsSnapshot;

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

        // Collect vertices for DNA tracking
        for (auto& pair : initialLocalPositions)
        {
            if (pair.first) {
                vertsSnapshot.push_back(pair.first);
            }
        }

        if (parentMesh && !isIdentity(totalAccumDelta))
        {
            if (auto* dna = parentMesh->getMeshDNA())
            {
                dna->trackFaceModify(totalAccumDelta, vertsSnapshot);
            }
        }

        // Clean up
        gizmoActive = false;
        initialLocalPositions.clear();
        totalAccumDelta = glm::mat4(1.0f);
    }
    
    wasUsingGizmoLastFrame = usingGizmo;
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
