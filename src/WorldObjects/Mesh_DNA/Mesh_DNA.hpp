#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <iostream>

class Vertice;
class Edge;
class Mesh;
class Face;  

enum class ComponentEditKind : uint8_t {
    None = 0,
    Edge,
    Vertice,
    Face,
    Extrude
};

struct ExtrudeRecord 
{
    Vertice* newVerts[4]{nullptr,nullptr,nullptr,nullptr};
    Edge* capEdges[4]{nullptr,nullptr,nullptr,nullptr};
    Edge* upEdges[4]{nullptr,nullptr,nullptr,nullptr};
    Face* sideFaces[4]{nullptr,nullptr,nullptr,nullptr};
    Face* capFace{nullptr};


    Vertice* oldVerts[4]{nullptr,nullptr,nullptr,nullptr};
    Edge* oldEdges[4]{nullptr,nullptr,nullptr,nullptr};

    float distance{0.f};
};


struct MeshTransformEvent 
{
    glm::mat4 delta{1.0f};
    uint64_t tick{0};
    std::string tag;

    bool isComponentEdit{false}; 

    ComponentEditKind kind{ComponentEditKind::None};
    std::vector<Vertice*> affectedVertices;

    ExtrudeRecord extrude{};
    uint64_t transformID{0};

};

struct SnapshotVertice {
    Vertice* ptr{nullptr};
    glm::vec3 local{0.0f};
    glm::vec3 world{0.0f};
};


class MeshDNA 
{

public:
    std::string uuid, name;
    void clear();

    void track(const glm::mat4& delta, uint64_t tick = 0, const std::string& tag = {});
    void trackWithAutoTick(const glm::mat4& delta, const std::string& tag);
    void trackWithTransformID(const glm::mat4& delta, const std::string& tag, uint64_t transformID);
    void trackEdgeModify(const glm::mat4& deltaWorld, const std::vector<Vertice*>& verts);
    void trackVerticeModify(const glm::mat4& deltaWorld, const std::vector<Vertice*>& verts);
    void trackFaceModify(const glm::mat4& deltaWorld, const std::vector<Vertice*>& verts);
    void trackExtrude(const ExtrudeRecord& rec);

    glm::mat4 accumulated() const;
    const std::vector<MeshTransformEvent>& getHistory() const; 

    glm::mat4 accumulatedUpTo(size_t count) const;

    void rewindToAndApply(size_t index_inclusive, Mesh* mesh);
    void rewindEdgeHistory(size_t index_inclusive, Mesh* mesh);
    void rewindVerticeHistory(size_t index_inclusive, Mesh* mesh);
    void rewindFaceHistory(size_t index_inclusive, Mesh* mesh);
    void rewindExtrudeHistory(size_t index_inclusive, Mesh* mesh);
    

    bool cancelTransformByID(uint64_t transformID, Mesh* mesh);

    size_t size() const { return history.size(); }
    void ensureInit(const glm::mat4& currentModel);

    void freezeFromMesh(const Mesh* mesh);  
    void refreezeFromMesh(const Mesh* mesh);   
    bool hasFreeze() const { return hasFrozen; }
    const glm::mat4& frozenModel() const { return frozenModelMatrix; }
    void resetToFreeze(Mesh* mesh) const; 


    void trackTranslate(const glm::mat4& delta) { trackWithAutoTick(delta, "translate"); }
    void trackRotate(const glm::mat4& delta) { trackWithAutoTick(delta, "rotate"); }
    void trackScale(const glm::mat4& delta) { trackWithAutoTick(delta, "scale"); }

    void setVerticeCount(size_t count) { verticeCount = count; }
    void setEdgeCount(size_t count) { edgeCount = count; }
    void setQuadCount(size_t count) { quadCount = count; }
    void setTriangleCount(size_t count) { triangleCount = count; }
    void setNgonCount(size_t count) { ngonCount = count; }

    size_t getVerticeCount() const { return verticeCount; }
    size_t getEdgeCount() const { return edgeCount; }
    size_t getQuadCount() const { return quadCount; }
    size_t getTriangleCount() const { return triangleCount; }
    size_t getNgonCount() const { return ngonCount; }

private:
    static inline bool isInitEvent(const MeshTransformEvent& ev) 
    {
        return !ev.isComponentEdit && ev.tag == "init";
    }

    std::vector<MeshTransformEvent> history;
    glm::mat4 acc{1.0f};
    bool hasInit{false};
    uint64_t nextTick{0};

    bool hasFrozen{false};
    glm::mat4 frozenModelMatrix{1.0f};
    std::vector<SnapshotVertice> frozenVertices;

    size_t verticeCount = 0;
    size_t edgeCount = 0;
    size_t quadCount = 0;
    size_t triangleCount = 0;
    size_t ngonCount = 0;

};