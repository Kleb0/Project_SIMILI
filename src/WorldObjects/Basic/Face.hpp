#pragma once

#include "WorldObjects/Basic/Vertice.hpp"
#include "WorldObjects/Basic/Edge.hpp"
#include <glm/glm.hpp>
#include <vector>

class Face
{
public:
    Face(Vertice* v0, Vertice* v1, Vertice* v2, Vertice* v3,
         Edge* e0, Edge* e1, Edge* e2, Edge* e3);
    ~Face();

    void initialize();
    void render(const glm::mat4& viewProj, const glm::mat4& modelMatrix);
    void destroy();

    const std::vector<Vertice*>& getVertices() const;
    const std::vector<Edge*>& getEdges() const;

private:
    std::vector<Vertice*> vertices;
    std::vector<Edge*> edges;

    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int shaderProgram = 0;

    void compileShaders();

    void uploadFromVertices();
};
