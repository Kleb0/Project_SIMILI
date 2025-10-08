


#pragma once
#include "WorldObjects/Basic/Face.hpp"
#include <array>


class Vertice;
class Edge;
class Mesh;

class Quad : public Face {
   
public:

    
    Quad(const std::array<Vertice*, 4>& vertices, const std::array<Edge*, 4>& edges);
    const std::array<Vertice*, 4>& getVerticesArray() const;
    const std::array<Edge*, 4>& getEdgesArray() const;
    std::array<Edge*, 4>& getEdgesNonConst() { return m_edges; }

    std::pair<Quad*, Quad*> splitQuad(Edge* edgeToSplit, Vertice* newVertex, Mesh* mesh);

    static Quad* createQuadFromVertices(Vertice* v0, Vertice* v1, Vertice* v2, Vertice* v3, Mesh* mesh);

    // Remplit les deux listes cutVerticesA et cutVerticesB à partir de 6 vertices
    void setCutVertices(const std::array<Vertice*, 3>& vertsA, const std::array<Vertice*, 3>& vertsB);
    void splitQuadFromCut(Mesh* mesh);

private:
    std::array<Vertice*, 4> m_vertices;
    std::array<Edge*, 4> m_edges;

    std::array<Vertice*, 3> cutVerticesA;
    std::array<Vertice*, 3> cutVerticesB;
};
