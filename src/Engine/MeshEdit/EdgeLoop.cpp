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



    

}
