#include "Engine/MeshEdit/EdgeLoop.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Basic/Quad.hpp"
#include "Engine/ThreeDScene.hpp"
#include <vector>
#include <unordered_set>
#include <imgui.h>

namespace MeshEdit 
{

    std::vector<Edge*> FindLoop(Vertice* startVert, Edge* selectedEdge, Mesh* mesh, ThreeDScene* scene, const ImVec2& oglChildPos, const ImVec2& oglChildSize)
    {
        std::vector<Edge*> loop;
        if (!startVert || !selectedEdge || !mesh) return loop;

        Quad* currentQuad = nullptr;
        int selectedEdgeIdx = -1;
        int startVertIdx = -1;
        for (Quad* quad : mesh->getQuads()) {
            const auto& verts = quad->getVerticesArray();
            const auto& edges = quad->getEdgesArray();
            for (int i = 0; i < 4; ++i) {
                if (verts[i] == startVert) startVertIdx = i;
                if (edges[i] == selectedEdge) selectedEdgeIdx = i;
            }
            if (startVertIdx != -1 && selectedEdgeIdx != -1) {
                currentQuad = quad;
                break;
            }
            startVertIdx = -1;
            selectedEdgeIdx = -1;
        }
        if (!currentQuad) return loop;

        Edge* currentEdge = selectedEdge;
        Vertice* currentVert = startVert;
        Quad* quad = currentQuad;
        std::unordered_set<Edge*> visited;
        std::unordered_set<Vertice*> visitedVerts;

        while (quad && currentEdge && !visited.count(currentEdge)) 
        {
            loop.push_back(currentEdge);
            visited.insert(currentEdge);
            visitedVerts.insert(currentVert);

            int edgeIdx = -1;
            const auto& edges = quad->getEdgesArray();
            for (int i = 0; i < 4; ++i) 
            {
                if (edges[i] == currentEdge) 
                {
                    edgeIdx = i;
                    break;
                }
            }
            if (edgeIdx == -1) break;

            int nextEdgeIdx = (edgeIdx + 2) % 4;
            Edge* nextEdge = edges[nextEdgeIdx];

            Vertice* nextVert = nullptr;
            if (nextEdge->getStart() != currentVert && !visitedVerts.count(nextEdge->getStart())) 
            {
                nextVert = nextEdge->getStart();
            } 
            else if (nextEdge->getEnd() != currentVert && !visitedVerts.count(nextEdge->getEnd())) 
            {
                nextVert = nextEdge->getEnd();
            } else 
            {
               
                break;
            }

            Quad* nextQuad = nullptr;
            for (Quad* q : mesh->getQuads()) 
            {
                if (q == quad) continue;
                const auto& qEdges = q->getEdgesArray();
                const auto& qVerts = q->getVerticesArray();
                bool hasEdge = false, hasVert = false;
                for (int i = 0; i < 4; ++i) 
                {
                    if (qEdges[i] == nextEdge) hasEdge = true;
                    if (qVerts[i] == nextVert) hasVert = true;
                }
                if (hasEdge && hasVert) 
                {
                    nextQuad = q;
                    break;
                }
            }

            currentEdge = nextEdge;
            currentVert = nextVert;
            quad = nextQuad;

            if (currentEdge == selectedEdge || currentVert == startVert) break;
        }

        CreatePerpendicularEdgeLoopGhost(loop, scene, oglChildPos, oglChildSize);
        return loop;
    }

    void CreatePerpendicularEdgeLoopGhost(const std::vector<Edge*>& loop, ThreeDScene* scene, const ImVec2& oglChildPos, const ImVec2& oglChildSize)
    {
        if (loop.empty() || !scene) return;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        glm::mat4 view = scene->getViewMatrix();
        glm::mat4 proj = scene->getProjectionMatrix();

        std::vector<glm::vec3> centers;
        for (Edge* e : loop)
        {
            if (!e) continue;
            Vertice* va = e->getStart();
            Vertice* vb = e->getEnd();
            glm::vec3 pa = va->getLocalPosition();
            glm::vec3 pb = vb->getLocalPosition();
            glm::vec3 center = 0.5f * (pa + pb);
            centers.push_back(center);
        }

        size_t n = centers.size();
        for (size_t i = 0; i < n; ++i)
        {
            glm::vec3 cA = centers[i];
            glm::vec3 cB = centers[(i+1)%n];
            glm::vec4 worldA(cA, 1.0f);
            glm::vec4 worldB(cB, 1.0f);
            glm::vec4 clipA = proj * view * worldA;
            glm::vec4 clipB = proj * view * worldB;
            if (clipA.w != 0.0f && clipB.w != 0.0f)
            {
                ImVec2 screenA = ImVec2(
                    oglChildPos.x + oglChildSize.x * (0.5f + 0.5f * (clipA.x / clipA.w)),
                    oglChildPos.y + oglChildSize.y * (0.5f - 0.5f * (clipA.y / clipA.w))
                );
                ImVec2 screenB = ImVec2(
                    oglChildPos.x + oglChildSize.x * (0.5f + 0.5f * (clipB.x / clipB.w)),
                    oglChildPos.y + oglChildSize.y * (0.5f - 0.5f * (clipB.y / clipB.w))
                );
                drawList->AddLine(screenA, screenB, IM_COL32(255,128,0,255), 4.0f); 
            }
        }
    }

}
