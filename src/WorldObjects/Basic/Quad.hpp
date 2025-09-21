#pragma once
#include "Face.hpp"
#include <array>

class Quad : public Face {
public:
    Quad(const std::array<Vertice*, 4>& vertices, const std::array<Edge*, 4>& edges);
    const std::array<Vertice*, 4>& getVerticesArray() const;
    const std::array<Edge*, 4>& getEdgesArray() const;
private:
    std::array<Vertice*, 4> m_vertices;
    std::array<Edge*, 4> m_edges;
};
