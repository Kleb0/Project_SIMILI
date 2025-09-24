#pragma once
#include <vector>
#include <list>
class Mesh;
class Edge;
class ThreeDScene;
struct ImVec2;

class Vertice; 

namespace MeshEdit 
{
     std::vector<Edge*> FindLoop(Vertice* startVert, Edge* selectedEdge, Mesh* mesh);

}



