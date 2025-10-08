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
        if (!selectedEdge || !mesh || !scene)
        {
            return loop;
        }

        Edge* currentEdge = selectedEdge;
        Quad* currentQuad = nullptr;
        const auto& meshQuads = mesh->getQuads();
        for (Quad* q : meshQuads)
        {
            const auto& edges = q->getEdgesArray();
            for (int i = 0; i < 4; ++i)
            {
                if (edges[i] == currentEdge)
                {
                    currentQuad = q;
                    break;
                }
            }
            if (currentQuad)
            {
                break;
            }
        }
        if (!currentQuad)
        {
            return loop;
        }

        traversedQuads.clear();
        std::unordered_set<Quad*> visitedQuads;

        // Détermination de la direction de parcours (comme dans CutQuad)
        glm::vec3 refDirA;
        bool referenceSet = false;

        auto getEdgeDir = [](Edge* e) -> glm::vec3
        {
            return e->getEnd()->getLocalPosition() - e->getStart()->getLocalPosition();
        };

        auto traverse = [&](Edge* edge, Quad* quad, int direction)
        {
            int safety = 0;
            while (quad && edge && safety < 100)
            {
                if (visitedQuads.count(quad))
                {
                    break;
                }
                loop.push_back(edge);
                traversedQuads.push_back(quad);
                visitedQuads.insert(quad);

                // Trouver l'index de l'edge courant
                int edgeIdx = -1;
                const auto& edgesArr = quad->getEdgesArray();
                for (int i = 0; i < 4; ++i)
                {
                    if (edgesArr[i] == edge)
                    {
                        edgeIdx = i;
                        break;
                    }
                }
                if (edgeIdx == -1)
                {
                    break;
                }

                // Détermination de la direction de l'edge courant
                glm::vec3 currentDir = getEdgeDir(edge);
                if (!referenceSet)
                {
                    refDirA = glm::normalize(currentDir);
                    referenceSet = true;
                }

                // Chercher l'edge parallèle (entry/exit) selon la direction
                int parallelIdx = -1;
                for (int i = 0; i < 4; ++i)
                {
                    if (i == edgeIdx)
                    {
                        continue;
                    }
                    glm::vec3 testDir = getEdgeDir(edgesArr[i]);
                    float dot = glm::dot(glm::normalize(testDir), refDirA);
                    // On veut l'edge le plus parallèle (dot proche de +1 ou -1)
                    if (std::abs(dot) > 0.99f)
                    {
                        parallelIdx = i;
                        break;
                    }
                }
                if (parallelIdx == -1)
                {
                    // Si pas d'edge parallèle, on est au bord (triangle ou fin)
                    break;
                }
                Edge* parallelEdge = edgesArr[parallelIdx];

                // Trouver le quad suivant qui partage cet edge
                Quad* nextQuad = nullptr;
                for (Quad* q : meshQuads)
                {
                    if (q == quad)
                    {
                        continue;
                    }
                    const auto& qEdges = q->getEdgesArray();
                    for (int i = 0; i < 4; ++i)
                    {
                        if (qEdges[i] == parallelEdge)
                        {
                            nextQuad = q;
                            break;
                        }
                    }
                    if (nextQuad)
                    {
                        break;
                    }
                }
                if (!nextQuad)
                {
                    break;
                }
                edge = parallelEdge;
                quad = nextQuad;
                ++safety;
            }
        };

        // Traverse forward
        traverse(currentEdge, currentQuad, 1);

        // Traverse backward (opposé)
        int startEdgeIdx = -1;
        const auto& startEdgesArr = currentQuad->getEdgesArray();
        for (int i = 0; i < 4; ++i)
        {
            if (startEdgesArr[i] == currentEdge)
            {
                startEdgeIdx = i;
                break;
            }
        }
        if (startEdgeIdx != -1)
        {
            int parallelIdx = -1;
            for (int i = 0; i < 4; ++i)
            {
                if (i == startEdgeIdx)
                {
                    continue;
                }
                glm::vec3 testDir = getEdgeDir(startEdgesArr[i]);
                float dot = glm::dot(glm::normalize(testDir), refDirA);
                if (std::abs(dot) > 0.99f)
                {
                    parallelIdx = i;
                    break;
                }
            }
            if (parallelIdx != -1)
            {
                Edge* parallelEdge = startEdgesArr[parallelIdx];
                Quad* nextQuad = nullptr;
                for (Quad* q : meshQuads)
                {
                    if (q == currentQuad)
                    {
                        continue;
                    }
                    const auto& qEdges = q->getEdgesArray();
                    for (int i = 0; i < 4; ++i)
                    {
                        if (qEdges[i] == parallelEdge)
                        {
                            nextQuad = q;
                            break;
                        }
                    }
                    if (nextQuad)
                    {
                        break;
                    }
                }
                if (nextQuad)
                {
                    traverse(parallelEdge, nextQuad, -1);
                }
            }
        }

        if (!loop.empty() && scene)
        {
            CreatePerpendicularEdgeLoopGhost(loop, scene, oglChildPos, oglChildSize);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_E))
        {
            MeshEdit::CutQuad(loop, mesh, traversedQuads);
            traversedQuads.clear();
            selectedEdge = nullptr;
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