// Mesh_DNA.cpp
#include "WorldObjects/Mesh_DNA/Mesh_DNA.hpp"

void MeshDNA::clear() {
    history.clear();
    acc = glm::mat4(1.0f);
}

void MeshDNA::track(const glm::mat4& delta, uint64_t tick, const std::string& tag) {
    history.push_back(MeshTransformEvent{delta, tick, tag});
    acc = delta * acc;
}

glm::mat4 MeshDNA::accumulated() const {
    return acc;

}

const std::vector<MeshTransformEvent>& MeshDNA::getHistory() const {
    return history;
}


glm::mat4 MeshDNA::accumulatedUpTo(size_t count) const
 {
    glm::mat4 a(1.0f);
    const size_t n = std::min(count, history.size());
    for (size_t i = 0; i < n; ++i)
    {
        a = history[i].delta * a;
    }
    return a;
}

void MeshDNA::rewindTo(size_t index_inclusive) {
    if (history.empty()) { acc = glm::mat4(1.0f); return; }
    if (index_inclusive + 1 > history.size()) return;

    history.resize(index_inclusive + 1);

    acc = glm::mat4(1.0f);
    for (size_t i = 0; i < history.size(); ++i) 
    {
        acc = history[i].delta * acc;
    }
}
