#pragma once
#include <vector>

class Mesh;
class Edge;

namespace MeshEdit 
{
     std::vector<Edge*> FindEdgeLoop(const Mesh& mesh, const Edge& startEdge);
}
   
