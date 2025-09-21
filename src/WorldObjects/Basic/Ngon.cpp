#include "Ngon.hpp"
#include <vector>
#include <cstddef>


Ngon::Ngon(const std::vector<int>& vertexIndices)
    : Face(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr), m_vertexIndices(vertexIndices) {}

Ngon::Ngon(const std::vector<Vertice*>& vertices, const std::vector<Edge*>& edges)
    : Face(
        vertices.size() > 0 ? vertices[0] : nullptr,
        vertices.size() > 1 ? vertices[1] : nullptr,
        vertices.size() > 2 ? vertices[2] : nullptr,
        vertices.size() > 3 ? vertices[3] : nullptr,
        edges.size() > 0 ? edges[0] : nullptr,
        edges.size() > 1 ? edges[1] : nullptr,
        edges.size() > 2 ? edges[2] : nullptr,
        edges.size() > 3 ? edges[3] : nullptr
      )
{
    this->vertices = vertices;
    this->edges = edges;
}

const std::vector<int>& Ngon::getVertexIndices() const {
    return m_vertexIndices;
}
