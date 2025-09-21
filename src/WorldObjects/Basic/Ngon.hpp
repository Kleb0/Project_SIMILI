#pragma once
#include "Face.hpp"
#include <vector>

class Ngon : public Face {
public:
    Ngon(const std::vector<int>& vertexIndices);
    Ngon(const std::vector<Vertice*>& vertices, const std::vector<Edge*>& edges);
    const std::vector<int>& getVertexIndices() const;
private:
    std::vector<int> m_vertexIndices;
};
