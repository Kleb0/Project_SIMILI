#include "Engine/MeshEdit/EdgeLoop.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Basic/Quad.hpp"
#include <vector>
#include <unordered_set>

namespace MeshEdit 
{

    static Quad* FindAdjacentQuad(const Mesh& mesh, const Edge* edge, const Quad* prevQuad) 
    {
        for (auto* face : mesh.getQuads()) {
            auto* quad = dynamic_cast<Quad*>(face);
            if (!quad) continue;
            const auto& edges = quad->getEdges();
            for (auto* e : edges) {
                if (e == edge && quad != prevQuad) {
                    return quad;
                }
            }
        }
        return nullptr;
    }


    std::vector<Edge*> FindEdgeLoop(const Mesh& mesh, const Edge& startEdge)
    {
        std::vector<Edge*> sideEdges;
        std::unordered_set<const Edge*> visitedLoop;
        const Edge* currentEdge = &startEdge;
        const Quad* prevQuad = nullptr;

        while (currentEdge && !visitedLoop.count(currentEdge)) 
        {
            const Quad* quad = FindAdjacentQuad(mesh, currentEdge, prevQuad);
            if (!quad) break;
            const auto& edges = quad->getEdges();
            int idx = -1;
            for (int i = 0; i < 4; ++i)
            {
                if (edges[i] == currentEdge) {
                    idx = i;
                    break;
                }
            }
            if (idx == -1) break;
            sideEdges.push_back(edges[(idx + 1) % 4]);
            sideEdges.push_back(edges[(idx + 3) % 4]);

            visitedLoop.insert(currentEdge);
            Edge* nextEdge = edges[(idx + 2) % 4];
            prevQuad = quad;
            currentEdge = nextEdge;
            if (currentEdge == &startEdge) break; 
        }
        return sideEdges;
    }

}
