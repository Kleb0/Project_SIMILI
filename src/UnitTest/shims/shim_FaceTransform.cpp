// src/UnitTest/shims/shim_FaceTransform.cpp
#include <list>
#include "Engine/MeshEdit/ExtrudeFace.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"

namespace FaceTransform {

Face* extrudeSelectedFace(std::list<Face*>& selectedFaces, float distance)
{
    if (selectedFaces.empty()) return nullptr;
    Face* target = selectedFaces.front();
    if (!target) return nullptr;

    const auto& vs = target->getVertices();
    if (vs.empty() || !vs[0]) return nullptr;

    ThreeDObject* owner = vs[0]->getMeshParent();
    auto* mesh = dynamic_cast<Mesh*>(owner);
    if (!mesh) return nullptr;

    auto& V = const_cast<std::vector<Vertice*>&>(mesh->getVertices());
    auto& E = const_cast<std::vector<Edge*>&>(mesh->getEdges());
    auto& F = const_cast<std::vector<Face*>&>(mesh->getFaces());

    const size_t facesBefore = F.size();

    MeshEdit::ExtrudeResult res{};
    if (!MeshEdit::extrudeQuadFace(mesh, V, E, F, target, distance, &res))
        return nullptr;

    if (auto* dna = mesh->getMeshDNA()) {
        ExtrudeRecord rec{};
        for (int i = 0; i < 4; ++i) {
            rec.newVerts[i] = res.newVerts[i];
            rec.capEdges[i] = res.capEdges[i];
            rec.upEdges[i] = res.upEdges[i];
            rec.sideFaces[i] = res.sideFaces[i];
            rec.oldVerts[i] = res.oldVerts[i];
            rec.oldEdges[i] = res.oldEdges[i];
        }
        rec.capFace = res.capFace;
        rec.distance = res.distance;
        dna->trackExtrude(rec);
    }

    if (F.size() <= facesBefore) return nullptr;
    Face* newCap = F.back();
    if (!newCap) return nullptr;


    selectedFaces.clear();
    selectedFaces.push_back(newCap);

    return newCap;
}

}
