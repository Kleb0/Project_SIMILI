#include <unordered_map>
#include <vector>
#include <string>
#include <utility>

#include <glm/glm.hpp>

#include "WorldObjects/Entities/ThreedObject.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Basic/Face.hpp"

//==================== ThreeDObject ====================//
namespace {
    struct ObjData {
        glm::mat4 model{1.0f};
    };
    static std::unordered_map<const ThreeDObject*, ObjData> g_obj;
}

ThreeDObject::ThreeDObject() { g_obj[this]; }
ThreeDObject::~ThreeDObject() { g_obj.erase(this); }

glm::mat4 ThreeDObject::getModelMatrix() const {
    auto it = g_obj.find(this);
    return (it!=g_obj.end()) ? it->second.model : glm::mat4(1.0f);
}
void ThreeDObject::setModelMatrix(const glm::mat4& m) {
    g_obj[this].model = m;
}

//==================== Vertice ====================//
namespace {
    struct VtxData {
        glm::vec3 local{0,0,0};
        glm::vec3 world{0,0,0};
        glm::vec4 color{1,1,1,1};
        std::string name;
        ThreeDObject* parent{nullptr};
    };
    static std::unordered_map<const Vertice*, VtxData> g_vtx;
}

Vertice::Vertice() { g_vtx[this]; }
Vertice::~Vertice() { g_vtx.erase(this); }

void Vertice::initialize() {}
void Vertice::destroy() {}
void Vertice::render(const glm::mat4&, const glm::mat4&) {}

void Vertice::setLocalPosition(const glm::vec3& p) { g_vtx[this].local = p; }
glm::vec3 Vertice::getLocalPosition() const {
    auto it = g_vtx.find(this);
    return (it!=g_vtx.end()) ? it->second.local : glm::vec3(0);
}

void Vertice::setPosition(const glm::vec3& p) { g_vtx[this].world = p; }
glm::vec3 Vertice::getPosition() const {
    auto it = g_vtx.find(this);
    return (it!=g_vtx.end()) ? it->second.world : glm::vec3(0);
}

void Vertice::setColor(const glm::vec4& c) { g_vtx[this].color = c; }
glm::vec4 Vertice::getColor() const {
    auto it = g_vtx.find(this);
    return (it!=g_vtx.end()) ? it->second.color : glm::vec4(1,1,1,1);
}

void Vertice::setName(const std::string& n) { g_vtx[this].name = n; }
const std::string& Vertice::getName() const {
    auto it = g_vtx.find(this);
    static const std::string kEmpty;
    return (it!=g_vtx.end()) ? it->second.name : kEmpty;
}

void Vertice::setMeshParent(ThreeDObject* p) { g_vtx[this].parent = p; }
ThreeDObject* Vertice::getMeshParent() const {
    auto it = g_vtx.find(this);
    return (it!=g_vtx.end()) ? it->second.parent : nullptr;
}

//==================== Edge ====================//
namespace {
    struct EdgeData {
        Vertice* a{nullptr};
        Vertice* b{nullptr};
    };
    static std::unordered_map<const Edge*, EdgeData> g_edge;
}

Edge::Edge(Vertice* a, Vertice* b) {
    g_edge[this] = EdgeData{a,b};
}
Edge::~Edge() { g_edge.erase(this); }

void Edge::initialize() {}
void Edge::destroy() {}
void Edge::render(const glm::mat4&, const glm::mat4&) {}

//==================== Face ====================//
namespace {
    struct FaceData {
        std::vector<Vertice*> verts;
        std::vector<Edge*>    edges;
        bool selected{false};
    };
    static std::unordered_map<const Face*, FaceData> g_face;
}

Face::Face(Vertice* v0, Vertice* v1, Vertice* v2, Vertice* v3,
           Edge* e0, Edge* e1, Edge* e2, Edge* e3)
{
    FaceData d;
    d.verts = {v0,v1,v2,v3};
    d.edges = {e0,e1,e2,e3};
    g_face[this] = std::move(d);
}

Face::~Face() { g_face.erase(this); }

void Face::initialize() {}
void Face::destroy() {}
void Face::render(const glm::mat4&, const glm::mat4&) {}

const std::vector<Vertice*>& Face::getVertices() const {
    auto it = g_face.find(this);
    if (it!=g_face.end()) return it->second.verts;
    static const std::vector<Vertice*> kEmpty;
    return kEmpty;
}
const std::vector<Edge*>& Face::getEdges() const {
    auto it = g_face.find(this);
    if (it!=g_face.end()) return it->second.edges;
    static const std::vector<Edge*> kEmpty;
    return kEmpty;
}