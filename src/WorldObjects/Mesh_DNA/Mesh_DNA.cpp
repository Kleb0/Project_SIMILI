#include "WorldObjects/Mesh_DNA/Mesh_DNA.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include <glm/gtc/matrix_inverse.hpp>

void MeshDNA::clear()
{
    history.clear();
    acc = glm::mat4(1.0f);
    hasInit = false;
    nextTick = 0;
}

void MeshDNA::track(const glm::mat4& delta, uint64_t tick, const std::string& tag) 
{
    MeshTransformEvent ev;
    ev.delta = delta;
    ev.tick = (tick ? tick : nextTick);
    ev.tag = tag;
    ev.isComponentEdit = false;

    history.push_back(ev);
    acc = delta * acc;

    if (ev.tick >= nextTick) nextTick = ev.tick + 1;
}

void MeshDNA::trackEdgeModify(const glm::mat4& deltaWorld, const std::vector<Vertice*>& verts) 
{
    MeshTransformEvent ev;
    ev.delta = deltaWorld;
    ev.tick = nextTick++;
    ev.tag = "edge_modify";
    ev.isComponentEdit = true;
    ev.affectedVertices = verts;

    history.push_back(std::move(ev));
}



void MeshDNA::trackWithAutoTick(const glm::mat4& delta, const std::string& tag) 
{
    track(delta, nextTick++, tag);
}

glm::mat4 MeshDNA::accumulated() const { return acc; }
const std::vector<MeshTransformEvent>& MeshDNA::getHistory() const { return history; }

void MeshDNA::ensureInit(const glm::mat4& currentModel) 
{
    if (hasInit) return;
    trackWithAutoTick(currentModel, "init");
    hasInit = true;
}


glm::mat4 MeshDNA::accumulatedUpTo(size_t count) const
{
    glm::mat4 a(1.0f);
    const size_t n = std::min(count, history.size());

    for (size_t i = 0; i < n; ++i)
    {
        const auto& ev = history[i];

        if (!ev.isComponentEdit) 
        {
            a = ev.delta * a;  
        }
    }
    return a;
}

void MeshDNA::rewindTo(size_t index_inclusive) 
{
    if (history.empty()) { acc = glm::mat4(1.0f); return; }
    if (index_inclusive + 1 > history.size()) return;

    history.resize(index_inclusive + 1);

    acc = glm::mat4(1.0f);
    for (size_t i = 0; i < history.size(); ++i) 
    {
        const auto& ev = history[i];
        if (!ev.isComponentEdit) {
            acc = ev.delta * acc;
        }
    }

    nextTick = history.back().tick + 1;
}

void MeshDNA::rewindEdgeHistory(size_t index_inclusive, Mesh* mesh)
{
    std::cout<< " Rewind Edge "<< std::endl;

    if (!mesh) return;
    if (history.empty()) { acc = glm::mat4(1.0f); return; }
    if (index_inclusive + 1 > history.size()) return;

    for (size_t k = history.size(); k-- > index_inclusive + 1; )
    {
        const auto& ev = history[k];
        if (ev.isComponentEdit)
        {
            const glm::mat4 invDelta = glm::inverse(ev.delta);
            for (auto* vtx : ev.affectedVertices)
            {
                if (!vtx) continue;
                ThreeDObject* parent = vtx->getMeshParent();
                if (!parent) continue;
                const glm::mat4 P  = parent->getModelMatrix();
                const glm::mat4 Pi = glm::inverse(P);
                glm::vec4 L  = glm::vec4(vtx->getLocalPosition(), 1.0f);
                glm::vec4 W  = P * L;
                glm::vec4 W2 = invDelta * W;
                glm::vec4 L2 = Pi * W2;
                vtx->setLocalPosition(glm::vec3(L2));
                vtx->setPosition(glm::vec3(W2));
            }
        }
    }

    history.resize(index_inclusive + 1);

    acc = glm::mat4(1.0f);
    for (size_t i = 0; i < history.size(); ++i)
    {
        const auto& ev = history[i];
        if (!ev.isComponentEdit)
            acc = ev.delta * acc;
    }

    nextTick = history.back().tick + 1;


}