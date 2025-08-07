#include "WorldObjects/Cube.hpp"
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

Cube::Cube() {}
Cube::~Cube()
{

}


void Cube::createVertices()
{
    glm::vec3 localPositions[] = {
        {-0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f},
        {-0.5f,  0.5f, -0.5f},
        {-0.5f, -0.5f,  0.5f},
        { 0.5f, -0.5f,  0.5f},
        { 0.5f,  0.5f,  0.5f},
        {-0.5f,  0.5f,  0.5f}
    };

    glm::mat4 modelMatrix = getModelMatrix();

    for (int i = 0; i < 8; ++i)
    {
        Vertice* vert = new Vertice();
        glm::vec4 worldPos = modelMatrix * glm::vec4(localPositions[i], 1.0f);
        vert->setPosition(glm::vec3(worldPos));
        vert->setLocalPosition(localPositions[i]); 
        vert->initialize();
        vert->setName("Vertice_" + std::to_string(i));
        vert->setMeshParent(this);
        vertices.push_back(vert);
    }
}


void Cube::createEdges()
{
    const int edgeIndices[][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}
    };

    for (const auto& pair : edgeIndices)
    {
        Edge* edge = new Edge(vertices[pair[0]], vertices[pair[1]]);
        edge->initialize();
        edges.push_back(edge);
    }
}

void Cube::createFaces()
{

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
        {2, 10, 6, 11},
        {3, 11, 7, 8},
        {1, 9, 5, 10}
    };

    for (int i = 0; i < 6; ++i)
    {
        Face* face = new Face(
            vertices[faceVertIndices[i][0]],
            vertices[faceVertIndices[i][1]],
            vertices[faceVertIndices[i][2]],
            vertices[faceVertIndices[i][3]],
            edges[faceEdgeIndices[i][0]],
            edges[faceEdgeIndices[i][1]],
            edges[faceEdgeIndices[i][2]],
            edges[faceEdgeIndices[i][3]]
        );
        face->initialize();
        faces.push_back(face);
    }


}

void Cube::initialize()
{
    createVertices();
    createEdges();
    createFaces();

    glm::mat4 modelMatrix = getModelMatrix();

    glm::vec3 localPositions[] = {
        {-0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f},
        {-0.5f,  0.5f, -0.5f},
        {-0.5f, -0.5f,  0.5f},
        { 0.5f, -0.5f,  0.5f},
        { 0.5f,  0.5f,  0.5f},
        {-0.5f,  0.5f,  0.5f}
    };

    unsigned int cubeIndices[] = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        0, 4, 7, 7, 3, 0,
        1, 5, 6, 6, 2, 1,
        3, 2, 6, 6, 7, 3,
        0, 1, 5, 5, 4, 0
    };

    std::vector<float> finalVertexData;
    for (int i = 0; i < 36; ++i)
    {
        glm::vec4 worldPos = modelMatrix * glm::vec4(localPositions[cubeIndices[i]], 1.0f);
        finalVertexData.push_back(worldPos.x);
        finalVertexData.push_back(worldPos.y);
        finalVertexData.push_back(worldPos.z);
    }
}


void Cube::render(const glm::mat4& viewProj)
{
    glm::mat4 modelMatrix = getModelMatrix();

    // --- Render Cube

    glm::vec3 localPositions[] = {
        {-0.5f, -0.5f, -0.5f},
        { 0.5f, -0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f},
        {-0.5f,  0.5f, -0.5f},
        {-0.5f, -0.5f,  0.5f},
        { 0.5f, -0.5f,  0.5f},
        { 0.5f,  0.5f,  0.5f},
        {-0.5f,  0.5f,  0.5f}
    };

    for (Face* face : faces)
    {
        face->render(viewProj, modelMatrix);
    }


    for (int i = 0; i < vertices.size(); ++i)
    {

        vertices[i]->render(viewProj, modelMatrix);
    }

    for (Edge* edge : edges)
    {
        edge->render(viewProj, modelMatrix);
    }
}

const std::vector<Vertice*>& Cube::getVertices() const
{
    return vertices;
}

const std::vector<Edge*>& Cube::getEdges() const
{
    return edges;
}

const std::vector<Face*>& Cube::getFaces() const
{
    return faces;
}

void Cube::destroy()
{

    for (Vertice* vert : vertices)
    {
        if (vert)
        {
            vert->destroy();
            delete vert;
        }
    }
    vertices.clear();

    for (Edge* edge : edges)
    {
        if (edge)
        {
            edge->destroy();
            delete edge;
        }
    }

    edges.clear();
    

    for (Face* face : faces)
    {
        if (face)
        {
            face->destroy();
            delete face;
        }
    }
    faces.clear();

    std::cout << "[Cube] Resources destroyed manually." << std::endl;
}
