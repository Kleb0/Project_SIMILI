#pragma once
#include "Engine/MeshEdit/CutQuad.hpp"

#include "WorldObjects/Entities/ThreedObject.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Basic/Face.hpp"
#include "WorldObjects/Basic/Quad.hpp"
#include "WorldObjects/Basic/Triangle.hpp"
#include "WorldObjects/Basic/Ngon.hpp"
#include "WorldObjects/Mesh_DNA/Mesh_DNA.hpp"

#include <vector>
#include <string>

namespace WorldObjects { namespace MeshNS {} }

class Mesh : public ThreeDObject
{

public:
    Mesh();
    ~Mesh() override;

    void initialize() override;
    void render(const glm::mat4& viewProj) override;
    void destroy() override;
    void updateGeometry();

    void setMeshDNA(MeshDNA* dna, bool takeOwnership = true);
    MeshDNA* getMeshDNA() const { return meshDNA; }
    bool ownsMeshDNA() const { return ownsDNA; }

    Vertice* addVertice(const glm::vec3& localPos, const std::string& name = {});
    Edge* addEdge(Vertice* a, Vertice* b);
    Face* addFace(Vertice* v0, Vertice* v1, Vertice* v2, Vertice* v3,
        Edge* e0 = nullptr, Edge* e1 = nullptr, Edge* e2 = nullptr, Edge* e3 = nullptr);

    Quad* addQuad(const std::array<Vertice*, 4>& vertices, const std::array<Edge*, 4>& edges);
    Triangle* addTriangle(Vertice* v0, Vertice* v1, Vertice* v2, Edge* e0 = nullptr, Edge* e1 = nullptr, Edge* e2 = nullptr);
    Ngon* addNgon(const std::vector<Vertice*>& vertices, const std::vector<Edge*>& edges);

    void finalize();

    const std::vector<Vertice*>& getVertices() const { return vertices; }
    const std::vector<Edge*>& getEdges() const { return edges; }
    const std::vector<Face*>& getFaces() const { return faces; }
    std::vector<Face*>& getFacesNonConst() { return faces; }

    std::vector<Quad*> getQuads() const;
    std::vector<Triangle*> getTriangles() const;
    std::vector<Ngon*> getNgons() const;

    bool hasTopology() const { return !vertices.empty() || !edges.empty() || !faces.empty(); }
    size_t vertexCount() const { return vertices.size(); }
    size_t edgeCount() const { return edges.size(); }
    size_t faceCount() const { return faces.size(); }

    void clearGeometry();
    std::vector<Edge*>& getEdgesNonConst() { return edges; }

private:
    std::vector<Vertice*> vertices;
    std::vector<Edge*> edges;
    std::vector<Face*> faces;

    MeshDNA* meshDNA = nullptr;
    bool ownsDNA = true;

    void destroyVertices();
    void destroyEdges();
    void destroyFaces();

    bool CanDisplayRenderMessage = true;
};
