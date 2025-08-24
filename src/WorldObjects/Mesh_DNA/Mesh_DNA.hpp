// Mesh_DNA.hpp
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <glm/glm.hpp>

class Vertice;
class Mesh;

enum class ComponentEditKind : uint8_t {
    None = 0,
    Edge,
    Vertice,
    Face
};


struct MeshTransformEvent 
{
    glm::mat4 delta{1.0f};
    uint64_t tick{0};
    std::string tag;

    bool isComponentEdit{false}; 
    ComponentEditKind kind{ComponentEditKind::None};
    std::vector<Vertice*> affectedVertices;

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
    void trackEdgeModify(const glm::mat4& deltaWorld, const std::vector<Vertice*>& verts);
    void trackVerticeModify(const glm::mat4& deltaWorld, const std::vector<Vertice*>& verts);
    void trackFaceModify(const glm::mat4& deltaWorld, const std::vector<Vertice*>& verts);

    glm::mat4 accumulated() const;
    const std::vector<MeshTransformEvent>& getHistory() const; 

    glm::mat4 accumulatedUpTo(size_t count) const;

    void rewindToAndApply(size_t index_inclusive, Mesh* mesh);
    void rewindEdgeHistory(size_t index_inclusive, Mesh* mesh);
    void rewindVerticeHistory(size_t index_inclusive, Mesh* mesh);
    void rewindFaceHistory(size_t index_inclusive, Mesh* mesh);

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
};
