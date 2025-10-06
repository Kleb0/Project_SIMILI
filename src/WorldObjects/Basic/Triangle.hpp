


#pragma once
#include "WorldObjects/Basic/Face.hpp"
#include <array>


class Vertice;
class Edge;

class Triangle : public Face {
public:
    Triangle(const std::array<int, 3>& vertexIndices);
    Triangle(Vertice* v0, Vertice* v1, Vertice* v2, Edge* e0 = nullptr, Edge* e1 = nullptr, Edge* e2 = nullptr);
    // Accès aux indices des sommets
    const std::array<int, 3>& getVertexIndices() const;
private:
    std::array<int, 3> m_vertexIndices;
};
