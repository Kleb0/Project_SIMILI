#include "WorldObjects/Mesh_DNA/Mesh_DNA.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include <glm/gtc/matrix_inverse.hpp>
#include <iostream>
#include <algorithm>
#include <iomanip> 

void MeshDNA::clear()
{
    history.clear();
    acc = glm::mat4(1.0f);
    hasInit = false;
    nextTick = 0;

    hasFrozen = false;
    frozenModelMatrix = glm::mat4(1.0f);
    frozenVertices.clear();
}


static void recomputeWorldFromLocal(Mesh* mesh)
{
    if (!mesh) return;
    const glm::mat4 P = mesh->getModelMatrix();
    for (Vertice* v : mesh->getVertices())
    {
        if (!v) continue;
        glm::vec4 W = P * glm::vec4(v->getLocalPosition(), 1.0f);
        v->setPosition(glm::vec3(W));
    }
}

static glm::mat4 modelFromFreezeUpTo(const std::vector<MeshTransformEvent>& history,
size_t index_inclusive, const glm::mat4& frozenModel, bool hasFrozen)
{
    glm::mat4 M = hasFrozen ? frozenModel : glm::mat4(1.0f);
    const size_t n = std::min(index_inclusive + 1, history.size());
    for (size_t i = 0; i < n; ++i)
    {
        const auto& ev = history[i];
        if (ev.isComponentEdit) continue; 
        if (hasFrozen && ev.tag == "init") continue; 
        M = ev.delta * M;
    }
    return M;
}

void MeshDNA::rewindToAndApply(size_t index_inclusive, Mesh* mesh)
{
    if (!mesh) return;

    if (history.empty()) 
    {
        if (hasFrozen) 
        {
            resetToFreeze(mesh);
        }
        acc = glm::mat4(1.0f);
        return;
    }

    if (index_inclusive + 1 > history.size())
        index_inclusive = history.size() - 1;


    if (hasFrozen) 
    {
        resetToFreeze(mesh); 
    } 
    else 
    {
        mesh->setModelMatrix(glm::mat4(1.0f));
        recomputeWorldFromLocal(mesh);
    }

    glm::mat4 M = modelFromFreezeUpTo(history, index_inclusive, frozenModelMatrix, hasFrozen);
    mesh->setModelMatrix(M);
    recomputeWorldFromLocal(mesh);

    history.resize(index_inclusive + 1);

    acc = glm::mat4(1.0f);
    for (const auto& ev : history)
    {
        if (!ev.isComponentEdit) 
        {
            if (hasFrozen && isInitEvent(ev)) continue;
            acc = ev.delta * acc;
        }
    }

    nextTick = history.back().tick + 1;
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
    ev.kind  = ComponentEditKind::Edge;  
    ev.affectedVertices = verts;

    history.push_back(std::move(ev));
}

void MeshDNA::trackVerticeModify(const glm::mat4& deltaWorld, const std::vector<Vertice*>& verts)
{
    MeshTransformEvent ev;
    ev.delta = deltaWorld;
    ev.tick  = nextTick++;
    ev.tag   = "vertex_modify";
    ev.isComponentEdit = true; 
    ev.kind  = ComponentEditKind::Vertice;
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


void MeshDNA::freezeFromMesh(const Mesh* mesh)
{
    if (hasFrozen || !mesh) return;

    frozenModelMatrix = mesh->getModelMatrix();
    frozenVertices.clear();
    frozenVertices.reserve(mesh->vertexCount());

    const auto& verts = mesh->getVertices();

    for (Vertice* v : verts)
    {
        if (!v) continue;
        SnapshotVertice snap;
        snap.ptr   = v;
        snap.local = v->getLocalPosition();
        snap.world = v->getPosition(); 
        frozenVertices.push_back(snap);
    }
    hasFrozen = true;

    const glm::vec3 pos = mesh->getPosition();
    const glm::vec3 rot = mesh->getRotation();
    const glm::vec3 scl = mesh->getScale();

    std::cout << std::fixed << std::setprecision(3)
            << "[MeshDNA] Freeze captured | "
            << "pos=(" << pos.x << ", " << pos.y << ", " << pos.z << ") "
            << "rot=(" << rot.x << "°, " << rot.y << "°, " << rot.z << "°) "
            << "scale=(" << scl.x << ", " << scl.y << ", " << scl.z << ")"
            << std::endl;
    }

void MeshDNA::refreezeFromMesh(const Mesh* mesh)
{
    hasFrozen = false;
    frozenVertices.clear();
    frozenModelMatrix = glm::mat4(1.0f);
    freezeFromMesh(mesh);
}

void MeshDNA::resetToFreeze(Mesh* mesh) const
{
    if (!hasFrozen || !mesh) return;

    mesh->setModelMatrix(frozenModelMatrix);


    for (const auto& snap : frozenVertices)
    {
        if (!snap.ptr) continue;

        snap.ptr->setLocalPosition(snap.local);

        const glm::mat4 P = mesh->getModelMatrix();
        glm::vec4 W = P * glm::vec4(snap.local, 1.0f);
        snap.ptr->setPosition(glm::vec3(W));
    }
}



glm::mat4 MeshDNA::accumulatedUpTo(size_t count) const
{
    glm::mat4 a(1.0f);
    const size_t n = std::min(count, history.size());

    for (size_t i = 0; i < n; ++i)
    {
        const auto& ev = history[i];

        if (ev.kind == ComponentEditKind::None)
        { 
            a = ev.delta * a;
        }
    }
    return a;
}


void MeshDNA::rewindEdgeHistory(size_t index_inclusive, Mesh* mesh)
{

    if (!mesh) return;
    if (history.empty()) { acc = glm::mat4(1.0f); return; }
    if (index_inclusive + 1 > history.size()) return;

    for (size_t k = history.size(); k-- > index_inclusive + 1; ) 
    {
        const auto& ev = history[k];
        if (ev.kind == ComponentEditKind::Edge) 
        {
            const glm::mat4 invDelta = glm::inverse(ev.delta);
            for (auto* vtx : ev.affectedVertices) 
            {
                if (!vtx) continue;
                ThreeDObject* parent = vtx->getMeshParent();
                if (!parent) continue;

                const glm::mat4 P = parent->getModelMatrix();
                const glm::mat4 Pi = glm::inverse(P);

                glm::vec4 L  = glm::vec4(vtx->getLocalPosition(), 1.0f);
                glm::vec4 W2 = invDelta * (P * L);
                glm::vec4 L2 = Pi * W2;

                vtx->setLocalPosition(glm::vec3(L2));
                vtx->setPosition(glm::vec3(W2));
            }
        }
    }

    size_t write = 0;

    for (size_t idx = 0; idx < history.size(); ++idx) 
    {
        bool drop = (idx > index_inclusive) && (history[idx].kind == ComponentEditKind::Edge);
        if (!drop) {
            history[write++] = std::move(history[idx]);
        }
    }
    history.resize(write);

    acc = glm::mat4(1.0f);
    for (const auto& ev : history)
    {
        if (ev.kind != ComponentEditKind::None) continue;
        if (hasFrozen && isInitEvent(ev)) continue; 
        acc = ev.delta * acc;
    }

    nextTick = history.empty() ? 0 : history.back().tick + 1;
}


void MeshDNA::rewindVerticeHistory(size_t index_inclusive, Mesh* mesh)
{

    if (!mesh) return;
    if (history.empty()) { acc = glm::mat4(1.0f); return; }
    if (index_inclusive + 1 > history.size()) return;

    for (size_t k = history.size(); k-- > index_inclusive + 1; ) 
    {
        const auto& ev = history[k];
        if (ev.kind == ComponentEditKind::Vertice)
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
                glm::vec4 W2 = invDelta * (P * L);
                glm::vec4 L2 = Pi * W2;

                vtx->setLocalPosition(glm::vec3(L2));
                vtx->setPosition(glm::vec3(W2));
            }
        }
    }

    size_t write = 0;
    for (size_t idx = 0; idx < history.size(); ++idx) 
    {
        bool drop = (idx > index_inclusive) && (history[idx].kind == ComponentEditKind::Vertice);
        if (!drop) {
            history[write++] = std::move(history[idx]);
        }
    }
    history.resize(write);

    acc = glm::mat4(1.0f);
    for (const auto& ev : history) 
    {
        if (ev.kind == ComponentEditKind::None)
            acc = ev.delta * acc;
    }

    nextTick = history.empty() ? 0 : history.back().tick + 1;
}

void MeshDNA::trackFaceModify(const glm::mat4& deltaWorld, const std::vector<Vertice*>& verts)
{
    MeshTransformEvent ev;
    ev.delta = deltaWorld;
    ev.tick  = nextTick++;
    ev.tag   = "face_modify";
    ev.isComponentEdit = true;
    ev.kind  = ComponentEditKind::Face;
    ev.affectedVertices = verts;
    history.push_back(std::move(ev));
}


void MeshDNA::rewindFaceHistory(size_t index_inclusive, Mesh* mesh)
{
    if (!mesh) return;
    if (history.empty()) { acc = glm::mat4(1.0f); return; }
    if (index_inclusive + 1 > history.size()) return;


    for (size_t k = history.size(); k-- > index_inclusive + 1; )
    {
        const auto& ev = history[k];
        if (ev.kind == ComponentEditKind::Face)
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
                glm::vec4 W2 = invDelta * (P * L);
                glm::vec4 L2 = Pi * W2;

                vtx->setLocalPosition(glm::vec3(L2));
                vtx->setPosition(glm::vec3(W2));
            }
        }
    }


    size_t write = 0;
    for (size_t idx = 0; idx < history.size(); ++idx)
    {
        bool drop = (idx > index_inclusive) && (history[idx].kind == ComponentEditKind::Face);
        if (!drop) history[write++] = std::move(history[idx]);
    }
    history.resize(write);

    acc = glm::mat4(1.0f);
    for (const auto& ev : history)
        if (ev.kind == ComponentEditKind::None)
            acc = ev.delta * acc;

    nextTick = history.empty() ? 0 : history.back().tick + 1;
}
