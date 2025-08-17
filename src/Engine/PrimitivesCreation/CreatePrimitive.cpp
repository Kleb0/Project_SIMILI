#include "Engine/PrimitivesCreation/CreatePrimitive.hpp"
#include <vector>
#include <iostream>

namespace Primitives
{
    Mesh* CreateCubeMesh(float size, const glm::vec3& center, const std::string& name, bool attachDNA)
    {
    
        auto* mesh = new Mesh();
        mesh->setName(name);
        mesh->setPosition(center);
        mesh->setScale(glm::vec3(size)); 

        const glm::vec3 localPositions[8] = {
            {-0.5f, -0.5f, -0.5f},
            { 0.5f, -0.5f, -0.5f},
            { 0.5f,  0.5f, -0.5f},
            {-0.5f,  0.5f, -0.5f},
            {-0.5f, -0.5f,  0.5f},
            { 0.5f, -0.5f,  0.5f},
            { 0.5f,  0.5f,  0.5f},
            {-0.5f,  0.5f,  0.5f}
        };

        std::vector<Vertice*> vs;
        vs.reserve(8);
        for (int i = 0; i < 8; ++i)
            vs.push_back(mesh->addVertice(localPositions[i], "Vertice_" + std::to_string(i)));

        const int edgeIndices[12][2] = {
            {0, 1}, {1, 2}, {2, 3}, {3, 0},
            {4, 5}, {5, 6}, {6, 7}, {7, 4},
            {0, 4}, {1, 5}, {2, 6}, {3, 7}
        };

        std::vector<Edge*> es;
        es.reserve(12);
        for (int i = 0; i < 12; ++i)
            es.push_back(mesh->addEdge(vs[edgeIndices[i][0]], vs[edgeIndices[i][1]]));


        const int faceVertIndices[6][4] = {
            {0, 1, 2, 3}, 
            {4, 5, 6, 7}, 
            {0, 4, 5, 1}, 
            {3, 2, 6, 7}, 
            {0, 3, 7, 4}, 
            {1, 5, 6, 2} 
        };

        const int faceEdgeIndices[6][4] = {
            {0, 1, 2, 3},
            {4, 5, 6, 7},
            {8, 4, 9, 0},
            {2,10, 6,11},
            {3,11, 7, 8},
            {1, 9, 5,10}
        };

        for (int i = 0; i < 6; ++i)
        {
            mesh->addFace(
                vs[faceVertIndices[i][0]],
                vs[faceVertIndices[i][1]],
                vs[faceVertIndices[i][2]],
                vs[faceVertIndices[i][3]],
                es[faceEdgeIndices[i][0]],
                es[faceEdgeIndices[i][1]],
                es[faceEdgeIndices[i][2]],
                es[faceEdgeIndices[i][3]]
            );
        }

        if (attachDNA)
        {
            auto* dna = new MeshDNA();
            dna->name = name;
            mesh->setMeshDNA(dna); 
        }


        mesh->finalize();

        return mesh;
        std::cout << "[CreatePrimitive.cpp] Create Primitive with name " << name << std::endl;
    }
}
