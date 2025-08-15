#pragma once
#include <vector>
#include <glm/glm.hpp>

class ThreeDObject;
class Vertice;
class Edge;
class Face;

namespace MeshEdit {

bool extrudeQuadFace(
    ThreeDObject* owner,
    std::vector<Vertice*>& vertices,
    std::vector<Edge*>& edges,
    std::vector<Face*>& faces,
    Face* target,
    float distance
);

} 
