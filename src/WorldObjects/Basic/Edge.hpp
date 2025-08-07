#pragma once

#include "WorldObjects/Basic/Vertice.hpp"
#include <glm/glm.hpp>

class Edge
{
public:
    Edge(Vertice* start, Vertice* end);
    ~Edge();

    void initialize();
    void render(const glm::mat4& viewProj, const glm::mat4& modelMatrix);
    void destroy();

    Vertice* getStart() const;
    Vertice* getEnd() const;

private:
    Vertice* v1;
    Vertice* v2;

    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int shaderProgram = 0;

    void compileShaders();
};
