#include "Triangle.hpp"

Triangle::Triangle(const std::array<int, 3>& vertexIndices)
    : Face(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr), m_vertexIndices(vertexIndices) {}

Triangle::Triangle(Vertice* v0, Vertice* v1, Vertice* v2, Edge* e0, Edge* e1, Edge* e2)
    : Face(v0, v1, v2, nullptr, e0, e1, e2, nullptr)
{
    this->vertices = {v0, v1, v2};
    if (e0) this->edges.push_back(e0);
    if (e1) this->edges.push_back(e1);
    if (e2) this->edges.push_back(e2);
}

const std::array<int, 3>& Triangle::getVertexIndices() const {
    return m_vertexIndices;
}
