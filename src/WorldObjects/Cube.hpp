#pragma once

#include "WorldObjects/ThreedObject.hpp"
#include "WorldObjects/Vertice.hpp"
#include "WorldObjects/Edge.hpp"
#include "WorldObjects/Face.hpp"
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
    const std::vector<Face*>& getFaces() const;


private:
    std::vector<Vertice*> vertices;
    std::vector<Edge*> edges;
    std::vector<Face*> faces;

    void compileShaders();
    void createVertices();
    void createEdges();
    void createFaces();
};
