
#pragma once
#include <vector>
#include <list>
#include "WorldObjects/Basic/Quad.hpp"
class Mesh;
class Edge;
class ThreeDScene;
struct ImVec2;

class Vertice; 

namespace MeshEdit 
{

     extern std::vector<Quad*> traversedQuads;

     std::vector<Edge*> FindLoop(Vertice* startVert, Edge* selectedEdge, Mesh* mesh, ThreeDScene* scene, const ImVec2& oglChildPos, const ImVec2& oglChildSize);

     void CreatePerpendicularEdgeLoopGhost(const std::vector<Edge*>& loop, ThreeDScene* scene, const ImVec2& oglChildPos, const ImVec2& oglChildSize);

     void createGhostVertices(const std::vector<Edge*>& loop, ThreeDScene* scene, const ImVec2& oglChildPos, const ImVec2& oglChildSize);

}



