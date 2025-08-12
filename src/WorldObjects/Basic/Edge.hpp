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

    void setSelected(bool isSelected);
    bool isSelected() const;

    void setColor(const glm::vec4& c);
    glm::vec4 getColor() const;

private:
    Vertice* v1;
    Vertice* v2;

    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int shaderProgram = 0;

    glm::vec4 color = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); 
    bool edgeSelected = false;

    void compileShaders();
};
