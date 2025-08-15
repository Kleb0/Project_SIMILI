#include "Engine/MeshEdit/ExtrudeFace.hpp"
#include "WorldObjects/Entities/ThreedObject.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Basic/Face.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

namespace {


glm::vec3 computeFaceNormalLocal(const Face* f) {
    const auto& vs = f->getVertices();
    const glm::vec3 p0 = vs[0]->getLocalPosition();
    const glm::vec3 p1 = vs[1]->getLocalPosition();
    const glm::vec3 p2 = vs[2]->getLocalPosition();

    const glm::vec3 u = p1 - p0;
    const glm::vec3 v = p2 - p0;

    glm::vec3 n = glm::cross(u, v);
    const float len2 = glm::dot(n, n);
    if (len2 < 1e-12f) {
        return glm::vec3(0, 0, 1);
    }
    return n / std::sqrt(len2);
}

Edge* makeEdge(Vertice* a, Vertice* b) {
    Edge* e = new Edge(a, b);
    e->initialize();
    return e;
}

Face* makeFaceQuad(Vertice* v0, Vertice* v1, Vertice* v2, Vertice* v3,
                   Edge* e0, Edge* e1, Edge* e2, Edge* e3) {
    Face* f = new Face(v0, v1, v2, v3, e0, e1, e2, e3);
    f->initialize();
    return f;
}

void syncWorldFromLocal(Vertice* v, const glm::mat4& parentModel) {
    const glm::vec4 wp = parentModel * glm::vec4(v->getLocalPosition(), 1.0f);
    v->setPosition(glm::vec3(wp));
}

} 


namespace MeshEdit {

bool extrudeQuadFace(
    ThreeDObject* owner,
    std::vector<Vertice*>& vertices,
    std::vector<Edge*>& edges,
    std::vector<Face*>& faces,
    Face* target,
    float distance
) {
    if (!owner || !target) return false;

    const auto& oldVs = target->getVertices();
    const auto& oldEs = target->getEdges();
    if (oldVs.size() != 4 || oldEs.size() != 4) {
        return false;
    }

    // 1) Calcul of local normal
    const glm::vec3 nLocal = computeFaceNormalLocal(target);
    const glm::vec3 offset = nLocal * distance;

    Vertice* nv[4] = {nullptr, nullptr, nullptr, nullptr};
    for (int i = 0; i < 4; ++i) {
        Vertice* src = oldVs[i];
        auto* v = new Vertice();
        v->initialize();
        v->setMeshParent(owner);
        v->setLocalPosition(src->getLocalPosition() + offset);
        // sync world
        syncWorldFromLocal(v, owner->getModelMatrix());
        v->setName(src->getName() + "_extruded");
        v->setColor(src->getColor());
        vertices.push_back(v);
        nv[i] = v;
    }


    Edge* capE[4] = {
        makeEdge(nv[0], nv[1]),
        makeEdge(nv[1], nv[2]),
        makeEdge(nv[2], nv[3]),
        makeEdge(nv[3], nv[0]),
    };
    for (int i = 0; i < 4; ++i) edges.push_back(capE[i]);

    Edge* upE[4] = {
        makeEdge(oldVs[0], nv[0]),
        makeEdge(oldVs[1], nv[1]),
        makeEdge(oldVs[2], nv[2]),
        makeEdge(oldVs[3], nv[3]),
    };
    for (int i = 0; i < 4; ++i) edges.push_back(upE[i]);


    Face* sideF[4] = {nullptr, nullptr, nullptr, nullptr};

    for (int i = 0; i < 4; ++i) {
        const int i1 = (i + 1) & 3;

        Vertice* v0 = oldVs[i];
        Vertice* v1 = oldVs[i1];
        Vertice* v2 = nv[i1];
        Vertice* v3 = nv[i];

        Edge* eBase = oldEs[i];
        Edge* eUp1  = upE[i1];
        Edge* eTop  = capE[i];
        Edge* eUp0  = upE[i];

        sideF[i] = makeFaceQuad(v0, v1, v2, v3, eBase, eUp1, eTop, eUp0);
        faces.push_back(sideF[i]);
    }

  
    Face* cap = makeFaceQuad(nv[0], nv[1], nv[2], nv[3],
                             capE[0], capE[1], capE[2], capE[3]);
    faces.push_back(cap);
    return true;
}

} 
