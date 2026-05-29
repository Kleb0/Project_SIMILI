#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "Engine/MeshEdit/CutQuad.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <random>
#include <sstream>

const char* kEdgeVertexShaderGLSL = R"(
#version 450
layout(location = 0) in vec3 aPosition;
layout(push_constant) uniform PushData {
    mat4 mvp;
} push;
void main() {
    gl_Position = push.mvp * vec4(aPosition, 1.0);
}
)";

const char* kEdgeFragmentShaderGLSL = R"(
#version 450
layout(location = 0) out vec4 outColor;
void main() {
    outColor = vec4(0.0, 0.0, 0.0, 1.0); // noir
}
)";

Edge::Edge(Vertice* start, Vertice* end)
    : v1(start), v2(end)
{
    id = generateEdgeID();
}

std::string Edge::generateEdgeID()
{
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static const size_t idLength = 12;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);
    std::stringstream ss;
    for (size_t i = 0; i < idLength; ++i)
        ss << charset[dis(gen)];
    return ss.str();
}

Edge::~Edge()
{
    destroy();
}

void Edge::initialize()
{
    std::cout << "[Edge] Initialize called for edge ID: " << id << std::endl;
}

void Edge::render(const glm::mat4& viewProj, const glm::mat4& modelMatrix)
{
}

void Edge::destroy()
{
}

Vertice* Edge::getStart() const { return v1; }
Vertice* Edge::getEnd() const { return v2; }

void Edge::setSelected(bool isSelected) { edgeSelected = isSelected; }
bool Edge::isSelected() const { return edgeSelected; }

void Edge::setColor(const glm::vec4& c) { color = c; }
glm::vec4 Edge::getColor() const { return color; }


void Edge::setSharedFaces(const std::vector<Face*>& faces)
{
    sharedFaces = faces;
    for (const auto& f : faces)
    {
        if (dynamic_cast<class Quad*>(f))
        {
            quadEdge = true;
            break;
        }
    }
}



const std::vector<Face*>& Edge::getSharedFaces() const
{
    return sharedFaces;
}

std::vector<Vertice*> Edge::insertVerticesAlongEdge(int count, Mesh* parentMesh)
{
    std::vector<Vertice*> newVertices;
    if (!v1 || !v2 || !parentMesh || count < 1) return newVertices;

    glm::vec3 p1 = v1->getLocalPosition();
    glm::vec3 p2 = v2->getLocalPosition();
    for (int i = 1; i <= count; ++i)
    {
        float t = float(i) / float(count + 1);
        glm::vec3 pos = (1.0f - t) * p1 + t * p2;
        Vertice* v = parentMesh->addVertice(pos);
        newVertices.push_back(v);
    }
    return newVertices;
}

void Edge::splitEdge(Vertice* newVertice, Mesh* parentMesh)
{
    if (!v1 || !v2 || !newVertice || !parentMesh) return;

    Edge* e1 = parentMesh->addEdge(v1, newVertice);
    Edge* e2 = parentMesh->addEdge(newVertice, v2);
    for (Face* f : parentMesh->getFaces())
    {
        if (!f) continue;
        auto& faceEdges = f->getEdgesNonConst();
        for (size_t i = 0; i < faceEdges.size(); ++i)
        {
            if (faceEdges[i] == this)
            {
                faceEdges[i] = e1;
                break;
            }
        
        }
    }

    e1->initialize();
    e2->initialize();

    auto& meshEdges = parentMesh->getEdgesNonConst();
    auto it = std::find(meshEdges.begin(), meshEdges.end(), this);
    if (it != meshEdges.end())
    {
        meshEdges.erase(it);
    }
    
    this->destroy();
}