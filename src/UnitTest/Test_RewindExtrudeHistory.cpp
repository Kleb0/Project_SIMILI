// src/UnitTest/Test_RewindExtrudeHistory.cpp
#include <gtest/gtest.h>

#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Mesh_DNA/Mesh_DNA.hpp"

#include <list>
#include <algorithm>
#include <unordered_set>

class Face;
class Vertice;
class Edge;

namespace FaceTransform {
    Face* extrudeSelectedFace(std::list<Face*>& selectedFaces, float distance);
}

static bool containsPtr(const std::vector<Vertice*>& vec, const Vertice* p) {
    return std::find(vec.begin(), vec.end(), p) != vec.end();
}
static bool containsPtr(const std::vector<Edge*>& vec, const Edge* p) {
    return std::find(vec.begin(), vec.end(), p) != vec.end();
}
static bool containsPtr(const std::vector<Face*>& vec, const Face* p) {
    return std::find(vec.begin(), vec.end(), p) != vec.end();
}

TEST(MeshExtrudeDNA, RewindExtrudeHistory_RestoresTopology)
{

    auto* dna = new MeshDNA();
    Mesh mesh;
    mesh.setMeshDNA(dna, true);

    auto* v0 = mesh.addVertice({0,0,0}, "v0");
    auto* v1 = mesh.addVertice({1,0,0}, "v1");
    auto* v2 = mesh.addVertice({1,1,0}, "v2");
    auto* v3 = mesh.addVertice({0,1,0}, "v3");
    ASSERT_EQ(mesh.vertexCount(), 4u);

    auto* e0 = mesh.addEdge(v0, v1);
    auto* e1 = mesh.addEdge(v1, v2);
    auto* e2 = mesh.addEdge(v2, v3);
    auto* e3 = mesh.addEdge(v3, v0);
    ASSERT_EQ(mesh.edgeCount(), 4u);

    auto* f0 = mesh.addFace(v0, v1, v2, v3, e0, e1, e2, e3);
    ASSERT_NE(f0, nullptr);
    ASSERT_EQ(mesh.faceCount(), 1u);


    mesh.finalize();
    ASSERT_TRUE(dna->hasFreeze());
    ASSERT_GE(dna->size(), 1u);
    {
        const auto& hist = dna->getHistory();
        EXPECT_EQ(hist.front().tag, "init");
    }


    std::list<Face*> selected{f0};
    const float dist = 0.2f;
    Face* newCap = FaceTransform::extrudeSelectedFace(selected, dist);
    ASSERT_NE(newCap, nullptr) << "Extrusion must return the cap face";


    EXPECT_EQ(mesh.vertexCount(), 8u);
    EXPECT_EQ(mesh.edgeCount(), 12u);
    EXPECT_EQ(mesh.faceCount(), 5u);


    ASSERT_GE(dna->size(), 2u);
    const auto& hist = dna->getHistory();
    const size_t extrIndex = hist.size() - 1;
    const auto& extr = hist.back();

    EXPECT_EQ(extr.tag, "extrude_face");
    EXPECT_EQ(extr.kind, ComponentEditKind::Extrude);
    EXPECT_FLOAT_EQ(extr.extrude.distance, dist);
    for (int i = 0; i < 4; ++i) {
        EXPECT_NE(extr.extrude.newVerts[i],  nullptr);
        EXPECT_NE(extr.extrude.capEdges[i],  nullptr);
        EXPECT_NE(extr.extrude.upEdges[i],   nullptr);
        EXPECT_NE(extr.extrude.sideFaces[i], nullptr);
        EXPECT_NE(extr.extrude.oldVerts[i],  nullptr);
        EXPECT_NE(extr.extrude.oldEdges[i],  nullptr);
    }
    EXPECT_NE(extr.extrude.capFace, nullptr);

    std::unordered_set<Vertice*> createdVerts;
    std::unordered_set<Edge*> createdEdges;
    std::unordered_set<Face*> createdFaces;

    for (int i = 0; i < 4; ++i) {
        createdVerts.insert(extr.extrude.newVerts[i]);
        createdEdges.insert(extr.extrude.capEdges[i]);
        createdEdges.insert(extr.extrude.upEdges[i]);
        createdFaces.insert(extr.extrude.sideFaces[i]);
    }
    createdFaces.insert(extr.extrude.capFace);


    if (extrIndex > 0) {
        dna->rewindExtrudeHistory(extrIndex - 1, &mesh);
    }


    EXPECT_EQ(mesh.faceCount(),   1u);
    EXPECT_EQ(mesh.vertexCount(), 4u);
    EXPECT_EQ(mesh.edgeCount(),   4u);

    const auto& V = mesh.getVertices();
    const auto& E = mesh.getEdges();
    const auto& F = mesh.getFaces();

    for (auto* nv : createdVerts)  { EXPECT_FALSE(containsPtr(V, nv)) << "Dangling extruded vert still present"; }
    for (auto* ne : createdEdges)  { EXPECT_FALSE(containsPtr(E, ne)) << "Dangling extruded edge still present"; }
    for (auto* nf : createdFaces)  { EXPECT_FALSE(containsPtr(F, nf)) << "Dangling extruded face still present"; }


    ASSERT_EQ(F.size(), 1u);
    const Face* restored = F.back();
    ASSERT_NE(restored, nullptr);

    {
        const auto& vs = restored->getVertices();
        ASSERT_EQ(vs.size(), 4u);

        EXPECT_TRUE(containsPtr(const_cast<std::vector<Vertice*>&>(vs), v0));
        EXPECT_TRUE(containsPtr(const_cast<std::vector<Vertice*>&>(vs), v1));
        EXPECT_TRUE(containsPtr(const_cast<std::vector<Vertice*>&>(vs), v2));
        EXPECT_TRUE(containsPtr(const_cast<std::vector<Vertice*>&>(vs), v3));
    }


    const size_t newHistSize = dna->size();
    EXPECT_LT(newHistSize, extrIndex + 1);
    if (newHistSize > 0) {
        EXPECT_NE(dna->getHistory().back().tag, "extrude_face");
    }
}
