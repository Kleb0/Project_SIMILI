// Mesh_DNA.hpp
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <glm/glm.hpp>

struct MeshTransformEvent {
    glm::mat4 delta{1.0f};
    uint64_t tick{0};
    std::string tag;
};

class MeshDNA {
public:
    std::string uuid, name;
    void clear();
    void track(const glm::mat4& delta, uint64_t tick = 0, const std::string& tag = {});
    glm::mat4 accumulated() const;
    const std::vector<MeshTransformEvent>& getHistory() const; 

    glm::mat4 accumulatedUpTo(size_t count) const;
    void rewindTo(size_t index_inclusive); 
    size_t size() const { return history.size(); }

private:
    std::vector<MeshTransformEvent> history;
    glm::mat4 acc{1.0f};
};
