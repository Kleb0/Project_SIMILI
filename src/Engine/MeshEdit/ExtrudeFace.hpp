#pragma once
#include <vector>
#include <glm/glm.hpp>

class ThreeDObject;
class Vertice;
class Edge;
class Face;

namespace MeshEdit {

struct ExtrudeResult {
    bool ok{false};
    Vertice* newVerts[4]{nullptr,nullptr,nullptr,nullptr};
    Edge*    capEdges[4]{nullptr,nullptr,nullptr,nullptr};
    Edge*    upEdges[4]{nullptr,nullptr,nullptr,nullptr};
    Face*    sideFaces[4]{nullptr,nullptr,nullptr,nullptr};
    Face*    capFace{nullptr};

    Vertice* oldVerts[4]{nullptr,nullptr,nullptr,nullptr};
    Edge*    oldEdges[4]{nullptr,nullptr,nullptr,nullptr};
    float    distance{0.f};
};



bool extrudeQuadFace(
    ThreeDObject* owner,
    std::vector<Vertice*>& vertices,
    std::vector<Edge*>& edges,
    std::vector<Face*>& faces,
    Face* target,
    float distance,
    ExtrudeResult* out
);

} 
