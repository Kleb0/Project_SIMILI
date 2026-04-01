#include "Engine/PrimitivesCreation/CreatePrimitive.hpp"
#include <vector>
#include <iostream>

namespace Primitives
{
    Mesh* CreateCubeMesh(float size, const glm::vec3& center, const std::string& name, bool attachDNA)
    {
        std::cout << "[CreatePrimitive] Creating mesh object..." << std::endl;
        auto* mesh = new Mesh();
        std::cout << "[CreatePrimitive] Mesh object created" << std::endl;
        
        mesh->setName(name);
        std::cout << "[CreatePrimitive] Mesh name set: " << name << std::endl;
        
        mesh->setPosition(center);
        std::cout << "[CreatePrimitive] Mesh position set" << std::endl;
        
        mesh->setScale(glm::vec3(size));
        std::cout << "[CreatePrimitive] Mesh scale set" << std::endl;

        std::cout << "[CreatePrimitive] Creating MeshDNA..." << std::endl;
        auto* dna = new MeshDNA();
        std::cout << "[CreatePrimitive] MeshDNA created" << std::endl;
        
        dna->name = name;
        mesh->setMeshDNA(dna);
        std::cout << "[CreatePrimitive] MeshDNA attached" << std::endl;

        const glm::vec3 localPositions[8] = 
        {
            {-0.5f, -0.5f, -0.5f},
            { 0.5f, -0.5f, -0.5f},
            { 0.5f,  0.5f, -0.5f},
            {-0.5f,  0.5f, -0.5f},
            {-0.5f, -0.5f,  0.5f},
            { 0.5f, -0.5f,  0.5f},
            { 0.5f,  0.5f,  0.5f},
            {-0.5f,  0.5f,  0.5f}
        };

        std::cout << "[CreatePrimitive] Adding vertices..." << std::endl;
        std::vector<Vertice*> vs;
        vs.reserve(8);
        for (int i = 0; i < 8; ++i)
        {
            vs.push_back(mesh->addVertice(localPositions[i], "Vertice_" + std::to_string(i)));
            if (attachDNA && mesh->getMeshDNA())
                mesh->getMeshDNA()->setVerticeCount(mesh->getMeshDNA()->getVerticeCount() + 1);
        }
        std::cout << "[CreatePrimitive] Vertices added: " << vs.size() << std::endl;

        const int edgeIndices[12][2] = 
        {
            {0, 1}, {1, 2}, {2, 3}, {3, 0},
            {4, 5}, {5, 6}, {6, 7}, {7, 4},
            {0, 4}, {1, 5}, {2, 6}, {3, 7}
        };

        std::cout << "[CreatePrimitive] Adding edges..." << std::endl;
        std::vector<Edge*> es;
        es.reserve(12);
        for (int i = 0; i < 12; ++i)
        {
            Edge* edge = mesh->addEdge(vs[edgeIndices[i][0]], vs[edgeIndices[i][1]]);
            es.push_back(edge);
            vs[edgeIndices[i][0]]->addEdge(edge);
            vs[edgeIndices[i][1]]->addEdge(edge);
            if (attachDNA && mesh->getMeshDNA())
                mesh->getMeshDNA()->setEdgeCount(mesh->getMeshDNA()->getEdgeCount() + 1);
        }
        std::cout << "[CreatePrimitive] Edges added: " << es.size() << std::endl;

        const int faceVertIndices[6][4] = 
        {
            {0, 1, 2, 3}, 
            {4, 5, 6, 7}, 
            {0, 4, 5, 1}, 
            {3, 2, 6, 7}, 
            {0, 3, 7, 4}, 
            {1, 5, 6, 2} 
        };

        const int faceEdgeIndices[6][4] = 
        {
            {0, 1, 2, 3},
            {4, 5, 6, 7},
            {8, 4, 9, 0},
            {2,10, 6,11},
            {3,11, 7, 8},
            {1, 9, 5,10}
        };

        std::cout << "[CreatePrimitive] Adding quads..." << std::endl;
        std::vector<Quad*> quads;
        quads.reserve(6);
        for (int i = 0; i < 6; ++i)
        {
            std::array<Vertice*, 4> quadVerts = 
            {
                vs[faceVertIndices[i][0]],
                vs[faceVertIndices[i][1]],
                vs[faceVertIndices[i][2]],
                vs[faceVertIndices[i][3]]
            };
            std::array<Edge*, 4> quadEdges = 
            {
                es[faceEdgeIndices[i][0]],
                es[faceEdgeIndices[i][1]],
                es[faceEdgeIndices[i][2]],
                es[faceEdgeIndices[i][3]]
            };
            Quad* quad = mesh->addQuad(quadVerts, quadEdges);
            quads.push_back(quad);
            if (attachDNA && mesh->getMeshDNA())
                mesh->getMeshDNA()->setQuadCount(mesh->getMeshDNA()->getQuadCount() + 1);
        }
        std::cout << "[CreatePrimitive] Quads added: " << quads.size() << std::endl;

        std::cout << "[CreatePrimitive] Setting shared faces for edges..." << std::endl;
        for (Edge* edge : es)
        {
            std::vector<Face*> sharedFaces;
            
            Vertice* vertA = edge->getStart();
            Vertice* vertB = edge->getEnd();
            
            std::vector<Edge*> edgesFromA;
            for (Edge* otherEdge : es)
            {
                if (otherEdge != edge && 
                    (otherEdge->getStart() == vertA || otherEdge->getEnd() == vertA))
                {
                    edgesFromA.push_back(otherEdge);
                }
            }
            
            std::vector<Edge*> edgesFromB;
            for (Edge* otherEdge : es)
            {
                if (otherEdge != edge && 
                    (otherEdge->getStart() == vertB || otherEdge->getEnd() == vertB))
                {
                    edgesFromB.push_back(otherEdge);
                }
            }
            
            for (Edge* edgeA : edgesFromA)
            {
                Vertice* otherVertA = (edgeA->getStart() == vertA) ? edgeA->getEnd() : edgeA->getStart();
                
                for (Edge* edgeB : edgesFromB)
                {
                    Vertice* otherVertB = (edgeB->getStart() == vertB) ? edgeB->getEnd() : edgeB->getStart();
                    
                    for (Edge* connectingEdge : es)
                    {
                        if ((connectingEdge->getStart() == otherVertA && connectingEdge->getEnd() == otherVertB) ||
                            (connectingEdge->getStart() == otherVertB && connectingEdge->getEnd() == otherVertA))
                        {
                            for (Quad* quad : quads)
                            {
                                const auto& quadEdges = quad->getEdgesArray();
                                bool hasEdge = false, hasEdgeA = false, hasEdgeB = false, hasConnecting = false;
                                
                                for (const auto& qe : quadEdges)
                                {
                                    if (qe == edge) hasEdge = true;
                                    if (qe == edgeA) hasEdgeA = true;
                                    if (qe == edgeB) hasEdgeB = true;
                                    if (qe == connectingEdge) hasConnecting = true;
                                }
                                
                                if (hasEdge && hasEdgeA && hasEdgeB && hasConnecting)
                                {
                                    bool alreadyAdded = false;
                                    for (Face* existingFace : sharedFaces)
                                    {
                                        if (existingFace == quad)
                                        {
                                            alreadyAdded = true;
                                            break;
                                        }
                                    }
                                    
                                    if (!alreadyAdded)
                                    {
                                        sharedFaces.push_back(quad);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            
            edge->setSharedFaces(sharedFaces);
        }
        std::cout << "[CreatePrimitive] Shared faces set for all edges" << std::endl;

        std::cout << "[CreatePrimitive] Finalizing mesh..." << std::endl;
        mesh->finalize();
        std::cout << "[CreatePrimitive] Mesh finalized" << std::endl;

        std::cout << "[CreatePrimitive] Cube mesh created successfully: " << name << std::endl;
        return mesh;
    }
}

