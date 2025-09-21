#include "Quad.hpp"

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
