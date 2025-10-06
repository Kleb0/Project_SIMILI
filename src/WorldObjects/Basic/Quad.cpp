#include "Quad.hpp"
#include "Engine/MeshEdit/CutQuad.hpp"
#include "Engine/ErrorBox.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "WorldObjects/Basic/Edge.hpp"
#include <iostream>


Quad::Quad(const std::array<Vertice*, 4>& vertices, const std::array<Edge*, 4>& edges)
    : Face(vertices[0], vertices[1], vertices[2], vertices[3], edges[0], edges[1], edges[2], edges[3]),
      m_vertices(vertices), m_edges(edges)
{
    this->vertices.assign(vertices.begin(), vertices.end());
    this->edges.assign(edges.begin(), edges.end());
}

const std::array<Vertice*, 4>& Quad::getVerticesArray() const 
{
    return m_vertices;
}

const std::array<Edge*, 4>& Quad::getEdgesArray() const
{
    return m_edges;
}

Quad* Quad::createQuadFromVertices(Vertice* v0, Vertice* v1, Vertice* v2, Vertice* v3, Mesh* mesh)
{
    if (!v0 || !v1 || !v2 || !v3 || !mesh) return nullptr;
    std::array<Vertice*, 4> quadVerts = {v0, v1, v2, v3};
    std::array<Edge*, 4> quadEdges = {
        mesh->addEdge(v0, v1),
        mesh->addEdge(v1, v2),
        mesh->addEdge(v2, v3),
        mesh->addEdge(v3, v0)
    };
    Quad* newQuad = new Quad(quadVerts, quadEdges);
    newQuad->initialize();
    mesh->getFacesNonConst().push_back(newQuad);
    return newQuad;
}

void Quad::setCutVertices(const std::array<Vertice*, 3>& vertsA, const std::array<Vertice*, 3>& vertsB)
{
    cutVerticesA = vertsA;
    cutVerticesB = vertsB;
}

void Quad::splitQuadFromCut(Mesh* mesh)
{
    // Crée un quad à partir des sommets du cut
    // Pour former un rectangle correct : A2 -> A3 -> B3 -> B2
    Quad* quad1 = Quad::createQuadFromVertices(
        cutVerticesA[1], // A2
        cutVerticesA[2], // A3  
        cutVerticesB[2], // B3
        cutVerticesB[1], // B2
        mesh);
}