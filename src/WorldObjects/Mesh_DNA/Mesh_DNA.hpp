// Mesh_DNA.hpp
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <glm/glm.hpp>

class Vertice;
class Mesh;

struct MeshTransformEvent 
{
    glm::mat4 delta{1.0f};
    uint64_t tick{0};
    std::string tag;

    bool isComponentEdit{false}; 
     std::vector<Vertice*> affectedVertices;

};

class MeshDNA 
{

public:
    std::string uuid, name;
    void clear();
    void track(const glm::mat4& delta, uint64_t tick = 0, const std::string& tag = {});

    void trackWithAutoTick(const glm::mat4& delta, const std::string& tag);


    void trackEdgeModify(const glm::mat4& deltaWorld, const std::vector<Vertice*>& verts);

    glm::mat4 accumulated() const;
    const std::vector<MeshTransformEvent>& getHistory() const; 

    glm::mat4 accumulatedUpTo(size_t count) const;

    void rewindTo(size_t index_inclusive); 
    void rewindEdgeHistory(size_t index_inclusive, Mesh* mesh);

    size_t size() const { return history.size(); }
    void ensureInit(const glm::mat4& currentModel);

    void trackTranslate(const glm::mat4& delta) { trackWithAutoTick(delta, "translate"); }
    void trackRotate(const glm::mat4& delta) { trackWithAutoTick(delta, "rotate"); }
    void trackScale(const glm::mat4& delta) { trackWithAutoTick(delta, "scale"); }

private:
    std::vector<MeshTransformEvent> history;
    glm::mat4 acc{1.0f};
    bool hasInit{false};
    uint64_t nextTick{0};
};
