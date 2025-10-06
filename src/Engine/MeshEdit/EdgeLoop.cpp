#include "Engine/MeshEdit/EdgeLoop.hpp"
#include "Engine/MeshEdit/CutQuad.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Basic/Quad.hpp"
#include "Engine/ThreeDScene.hpp"
#include <vector>
#include <unordered_set>
#include <imgui.h>

namespace MeshEdit 
{

    std::vector<Quad*> traversedQuads;

    std::vector<Edge*> FindLoop(Vertice* startVert, Edge* selectedEdge, Mesh* mesh, ThreeDScene* scene, const ImVec2& oglChildPos, const ImVec2& oglChildSize)
    {
        std::vector<Edge*> loop;
        if (!selectedEdge || !mesh || !scene) return loop;

        Edge* currentEdge = selectedEdge;
        loop.push_back(currentEdge);


        Quad* startQuad = nullptr;
        const auto& meshQuads = mesh->getQuads();
        for (Quad* q : meshQuads) {
            const auto& edges = q->getEdgesArray();
            for (int i = 0; i < 4; ++i) {
                if (edges[i] == currentEdge) {
                    startQuad = q;
                    break;
                }
            }
            if (startQuad) break;
        }
        if (!startQuad) return loop;

        Quad* currentQuad = startQuad;
        int edgeIdx = -1;
        const auto& edgesArr = currentQuad->getEdgesArray();
        for (int i = 0; i < 4; ++i) {
            if (edgesArr[i] == currentEdge) {
                edgeIdx = i;
                break;
            }
        }
        if (edgeIdx == -1) return loop;

        auto sharesVertice = [](Edge* e1, Edge* e2) {
            return (e1->getStart() == e2->getStart() || e1->getStart() == e2->getEnd() ||
                    e1->getEnd() == e2->getStart() || e1->getEnd() == e2->getEnd());
        };

        int nextEdgeIdx = -1;
        for (int i = 0; i < 4; ++i) {
            if (i == edgeIdx) continue;
            if (!sharesVertice(edgesArr[i], currentEdge)) {
                nextEdgeIdx = i;
                break;
            }
        }
        if (nextEdgeIdx == -1) return loop;

        Edge* nextEdge = edgesArr[nextEdgeIdx];

        Edge* firstEdge = currentEdge;
        Edge* oppEdge = nextEdge;
        Quad* quad = currentQuad;
        int safety = 0;
        traversedQuads.clear();
        std::unordered_set<Quad*> visitedQuads;
        traversedQuads.push_back(quad);
        visitedQuads.insert(quad);
        while (oppEdge && oppEdge != firstEdge && safety < 100) {
            loop.push_back(oppEdge);

            Quad* nextQuad = nullptr;
            for (Quad* q : meshQuads) {
                if (visitedQuads.count(q)) continue;
                const auto& qEdges = q->getEdgesArray();
                for (int i = 0; i < 4; ++i) {
                    if (qEdges[i] == oppEdge) {
                        nextQuad = q;
                        break;
                    }
                }
                if (nextQuad) break;
            }
            if (!nextQuad) break;

            traversedQuads.push_back(nextQuad);
            visitedQuads.insert(nextQuad);

            int oppIdx = -1;
            const auto& nextEdgesArr = nextQuad->getEdgesArray();
            for (int i = 0; i < 4; ++i) {
                if (nextEdgesArr[i] == oppEdge) {
                    oppIdx = i;
                    break;
                }
            }
            if (oppIdx == -1) break;
            int newOppIdx = -1;
            for (int i = 0; i < 4; ++i) {
                if (i == oppIdx) continue;
                if (!sharesVertice(nextEdgesArr[i], oppEdge)) {
                    newOppIdx = i;
                    break;
                }
            }
            if (newOppIdx == -1) break;

            quad = nextQuad;
            oppEdge = nextEdgesArr[newOppIdx];
            ++safety;
        }

        if (!loop.empty() && scene) 
        {
            CreatePerpendicularEdgeLoopGhost(loop, scene, oglChildPos, oglChildSize);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_E)) 
        {
            MeshEdit::CutQuad(loop, mesh, traversedQuads);
        }

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
            ThreeDObject* parent = va->getMeshParent() ? va->getMeshParent() : (vb ? vb->getMeshParent() : nullptr);
            glm::mat4 model = parent ? parent->getModelMatrix() : glm::mat4(1.0f);
            glm::vec3 pa = glm::vec3(model * glm::vec4(va->getLocalPosition(), 1.0f));
            glm::vec3 pb = glm::vec3(model * glm::vec4(vb->getLocalPosition(), 1.0f));
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

        createGhostVertices(loop, scene, oglChildPos, oglChildSize);
    }

    void createGhostVertices(const std::vector<Edge*>& loop, ThreeDScene* scene, const ImVec2& oglChildPos, const ImVec2& oglChildSize)
    {
        if (loop.empty() || !scene) return;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        glm::mat4 view = scene->getViewMatrix();
        glm::mat4 proj = scene->getProjectionMatrix();

        for (Edge* e : loop)
        {
            if (!e) continue;
            Vertice* va = e->getStart();
            Vertice* vb = e->getEnd();
            ThreeDObject* parent = va->getMeshParent() ? va->getMeshParent() : (vb ? vb->getMeshParent() : nullptr);
            glm::mat4 model = parent ? parent->getModelMatrix() : glm::mat4(1.0f);
            glm::vec3 pa = glm::vec3(model * glm::vec4(va->getLocalPosition(), 1.0f));
            glm::vec3 pb = glm::vec3(model * glm::vec4(vb->getLocalPosition(), 1.0f));
            glm::vec3 ghostPos = 0.5f * (pa + pb);
            glm::vec4 worldGhost(ghostPos, 1.0f);
            glm::vec4 clipGhost = proj * view * worldGhost;
            if (clipGhost.w != 0.0f)
            {
                ImVec2 screenGhost = ImVec2(
                    oglChildPos.x + oglChildSize.x * (0.5f + 0.5f * (clipGhost.x / clipGhost.w)),
                    oglChildPos.y + oglChildSize.y * (0.5f - 0.5f * (clipGhost.y / clipGhost.w))
                );
                drawList->AddCircleFilled(screenGhost, 7.0f, IM_COL32(255,0,0,255));
            }
        }
    }
}