#pragma once

#include "WorldObjects/Entities/ThreedObject.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Basic/Face.hpp"
#include "WorldObjects/Mesh_DNA/Mesh_DNA.hpp"
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

    void setMeshDNA(MeshDNA* dna);
    MeshDNA* getMeshDNA() const;


private:
    std::vector<Vertice*> vertices;
    std::vector<Edge*> edges;
    std::vector<Face*> faces;

    MeshDNA* meshDNA = nullptr;

    void createVertices();
    void createEdges();
    void createFaces();
};
