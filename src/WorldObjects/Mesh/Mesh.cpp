#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Basic/Quad.hpp"
#include "WorldObjects/Basic/Triangle.hpp"
#include "WorldObjects/Basic/Ngon.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

Mesh::Mesh()
{
    setIsMesh(true);
}

Mesh::~Mesh()
{
    destroy();
}

void Mesh::initialize()
{
    CanDisplayRenderMessage = true;
}

Quad* Mesh::addQuad(const std::array<Vertice*, 4>& vertices, const std::array<Edge*, 4>& edges)
{
    auto* quad = new Quad(vertices, edges);
    faces.push_back(quad);
    return quad;
}

Triangle* Mesh::addTriangle(Vertice* v0, Vertice* v1, Vertice* v2, Edge* e0, Edge* e1, Edge* e2)
{
    auto* tri = new Triangle(v0, v1, v2, e0, e1, e2);
    faces.push_back(tri);
    return tri;
}

Ngon* Mesh::addNgon(const std::vector<Vertice*>& vertices, const std::vector<Edge*>& edges)
{
    auto* ngon = new Ngon(vertices, edges);
    faces.push_back(ngon);
    return ngon;
}

void Mesh::render(const glm::mat4& viewProj)
{
    const glm::mat4 modelMatrix = getModelMatrix();

    for (Face* f : faces)
        if (f) f->render(viewProj, modelMatrix);

    for (Vertice* v : vertices)
        if (v) v->render(viewProj, modelMatrix);

    for (Edge* e : edges)
        if (e) e->render(viewProj, modelMatrix);

    if(CanDisplayRenderMessage)
    {
        std::cout << "[Mesh] Rendering completed (faces, vertices, edges)\n";
        CanDisplayRenderMessage = false;
    }
}

void Mesh::destroyVertices()
{
    for (Vertice* v : vertices)
    {
        if (!v) continue;
        v->destroy();
        delete v;
    }
    vertices.clear();
}

void Mesh::destroyEdges()
{
    for (Edge* e : edges)
    {
        if (!e) continue;
        e->destroy();
        delete e;
    }
    edges.clear();
}

void Mesh::destroyFaces()
{
    for (Face* f : faces)
    {
        if (!f) continue;
        f->destroy();
        delete f;
    }
    faces.clear();
}

void Mesh::destroy()
{
    destroyVertices();
    destroyEdges();
    destroyFaces();

    if (meshDNA && ownsDNA)
    {
        delete meshDNA;
    }
    meshDNA = nullptr;
    ownsDNA = true;

}

void Mesh::setMeshDNA(MeshDNA* dna, bool takeOwnership)
{
    if (meshDNA && ownsDNA && meshDNA != dna)
        delete meshDNA;

    meshDNA  = dna;
    ownsDNA  = takeOwnership;
}

Vertice* Mesh::addVertice(const glm::vec3& localPos, const std::string& name)
{
    auto* v = new Vertice();
    v->setLocalPosition(localPos);

    const glm::vec4 world = getModelMatrix() * glm::vec4(localPos, 1.0f);
    v->setPosition(glm::vec3(world));

    v->setMeshParent(this);
    if (!name.empty())
        v->setName(name);
    else
        v->setName("Vertice_" + std::to_string(vertices.size()));

    vertices.push_back(v);
    return v;
}

Edge* Mesh::addEdge(Vertice* a, Vertice* b)
{
    if (!a || !b) return nullptr;
    auto* e = new Edge(a, b);
    edges.push_back(e);
    return e;
}

Face* Mesh::addFace(Vertice* v0, Vertice* v1, Vertice* v2, Vertice* v3,
Edge* e0, Edge* e1, Edge* e2, Edge* e3)
{
    if (!v0 || !v1 || !v2 || !v3) return nullptr;

    auto* f = new Face(v0, v1, v2, v3, e0, e1, e2, e3);
    faces.push_back(f);
    return f;
}

std::vector<Quad*> Mesh::getQuads() const
{
    std::vector<Quad*> quads;
    for (Face* f : faces)
    {
        if (auto* q = dynamic_cast<Quad*>(f))
            quads.push_back(q);
    }
    return quads;
}

std::vector<Triangle*> Mesh::getTriangles() const
{
    std::vector<Triangle*> triangles;
    for (Face* f : faces)
    {
        if (auto* t = dynamic_cast<Triangle*>(f))
            triangles.push_back(t);
    }
    return triangles;
}

std::vector<Ngon*> Mesh::getNgons() const
{
    std::vector<Ngon*> ngons;
    for (Face* f : faces)
    {
        if (auto* n = dynamic_cast<Ngon*>(f))
            ngons.push_back(n);
    }
    return ngons;
}


void Mesh::finalize()
{

    for (Vertice* v : vertices)
    {

        v->setMeshParent(this);
        v->setName("Vertice_" + std::to_string(&v - &vertices[0]));
        v->initialize();
    }

    for (Edge* e : edges) e->initialize();
    for (Face* f : faces) f->initialize();

    meshDNA->ensureInit(getModelMatrix());
    meshDNA->freezeFromMesh(this);
}

void Mesh::clearGeometry()
{
    destroyVertices();
    destroyEdges();
    destroyFaces();
}

void Mesh::updateGeometry()
{
    std::cout << "[Mesh] Updating geometry...";

    for (Edge* e : edges)
        if (e) e->destroy();
    for (Face* f : faces)
        if (f) f->destroy();
    for (Vertice* v : vertices)
        if (v) v->destroy();

    for (Edge* e : edges)
        if (e) e->initialize();
    for (Face* f : faces)
        if (f) f->initialize();
    for (Vertice* v : vertices)
        if (v) v->initialize();
}
