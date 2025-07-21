#pragma once

#include "WorldObjects/ThreedObject.hpp"
#include "WorldObjects/Vertice.hpp"
#include "WorldObjects/Edge.hpp"
#include <vector>

class Cube : public ThreeDObject
{
public:
    Cube();
    ~Cube();

    void initialize() override;
    void render(const glm::mat4 &viewProj) override;
    void destroy() override;

    const std::vector<Vertice*>& getVertices() const;
    const std::vector<Edge*>& getEdges() const;

private:
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int shaderProgram = 0;

    std::vector<Vertice*> vertices;
    std::vector<Edge*> edges;
    void compileShaders();
    void createVertices();
    void createEdges();
};
