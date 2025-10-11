#include "Engine/MeshEdit/ExtrudeFace.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Entities/ThreedObject.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Basic/Face.hpp"
#include "WorldObjects/Basic/Quad.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

#include <type_traits>
#include <utility>
#include <engine/ErrorBox.hpp>

namespace MeshEdit
{
	template<typename T, typename = void>
	struct has_destroy : std::false_type {};

	template<typename T>
	struct has_destroy<T, std::void_t<decltype(std::declval<T*>()->destroy())>> : std::true_type {};

	glm::vec3 computeFaceNormalLocal(const Face* f) 
	{
		const auto& vs = f->getVertices();
		const glm::vec3 p0 = vs[0]->getLocalPosition();
		const glm::vec3 p1 = vs[1]->getLocalPosition();
		const glm::vec3 p2 = vs[2]->getLocalPosition();

		const glm::vec3 u = p1 - p0;
		const glm::vec3 v = p2 - p0;

		glm::vec3 n = glm::cross(u, v);
		const float len2 = glm::dot(n, n);
		if (len2 < 1e-12f) {
			return glm::vec3(0, 0, 1);
		}
		return n / std::sqrt(len2);
	}

	Edge* findOrMakeEdge(std::vector<Edge*>& edges, Vertice* a, Vertice* b) 
	{
		for (Edge* e : edges) {
			if ((e->getStart() == a && e->getEnd() == b) || (e->getStart() == b && e->getEnd() == a)) {
				return e;
			}
		}
		Edge* e = new Edge(a, b);
		e->initialize();
		edges.push_back(e);
		return e;
	}

	Quad* makeQuad(Vertice* v0, Vertice* v1, Vertice* v2, Vertice* v3,
	Edge* e0, Edge* e1, Edge* e2, Edge* e3) 
	{
		std::array<Vertice*, 4> quadVerts = {v0, v1, v2, v3};
		std::array<Edge*, 4> quadEdges = {e0, e1, e2, e3};
		Quad* q = new Quad(quadVerts, quadEdges);
		q->initialize();
		return q;
	}

	void syncWorldFromLocal(Vertice* v, const glm::mat4& parentModel) 
	{
		const glm::vec4 wp = parentModel * glm::vec4(v->getLocalPosition(), 1.0f);
		v->setPosition(glm::vec3(wp));
	}


	bool extrudeQuadFace(
		ThreeDObject* owner, std::vector<Vertice*>& vertices,
		std::vector<Edge*>& edges, std::vector<Face*>& faces,
		Face* target, float distance, ExtrudeResult* out
	) 
	{
		if (!owner || !target) return false;

		const auto& rVs = target->getVertices();
		const auto& rEs = target->getEdges();
		if (rVs.size() != 4 || rEs.size() != 4) return false;

		Vertice* oldV[4] = {rVs[0], rVs[1], rVs[2], rVs[3]};
		Edge* oldE[4] = {rEs[0], rEs[1], rEs[2], rEs[3]};

		glm::vec3 nLocal = computeFaceNormalLocal(target);

		Mesh* mesh = dynamic_cast<Mesh*>(owner);
		glm::vec3 meshCenter(0.0f);
		if (mesh) {
			const auto& meshVerts = mesh->getVertices();
			for (auto* v : meshVerts) meshCenter += v->getLocalPosition();
			if (!meshVerts.empty()) meshCenter /= float(meshVerts.size());
		}
		glm::vec3 faceCenter(0.0f);
		for (int i=0;i<4;++i) faceCenter += rVs[i]->getLocalPosition();
		faceCenter /= 4.0f;
		glm::vec3 toCenter = glm::normalize(meshCenter - faceCenter);

		if (glm::dot(nLocal, toCenter) > 0.0f) nLocal = -nLocal;
		const glm::vec3 offset = nLocal * distance;

		Vertice* nv[4] = {nullptr,nullptr,nullptr,nullptr};
		MeshDNA* dna = mesh ? mesh->getMeshDNA() : nullptr;

		for (int i=0;i<4;++i) 
		{
			Vertice* src = oldV[i];
			auto* v = new Vertice();
			v->initialize();
			v->setMeshParent(owner);
			v->setLocalPosition(src->getLocalPosition() + offset);
			syncWorldFromLocal(v, owner->getModelMatrix());
			v->setName(src->getName() + "_extruded");
			v->setColor(src->getColor());
			vertices.push_back(v);
			nv[i] = v;
		   dna->setVerticeCount(dna->getVerticeCount() + 1);
		}

		Edge* capE[4] = {
			findOrMakeEdge(edges, nv[0], nv[1]),
			findOrMakeEdge(edges, nv[1], nv[2]),
			findOrMakeEdge(edges, nv[2], nv[3]),
			findOrMakeEdge(edges, nv[3], nv[0]),
		};
		for (int i=0;i<4;++i) {
			// Associer les nouveaux edges aux vertices
			if (capE[i]) {
				nv[i]->addEdge(capE[i]);
				nv[(i+1)%4]->addEdge(capE[i]);
			}
			if (dna) dna->setEdgeCount(dna->getEdgeCount() + 1);
		}

		Edge* upE[4] = {
			findOrMakeEdge(edges, oldV[0], nv[0]),
			findOrMakeEdge(edges, oldV[1], nv[1]),
			findOrMakeEdge(edges, oldV[2], nv[2]),
			findOrMakeEdge(edges, oldV[3], nv[3]),
		};
		for (int i=0;i<4;++i) {
			// Associer les edges verticaux aux vertices
			if (upE[i]) {
				oldV[i]->addEdge(upE[i]);
				nv[i]->addEdge(upE[i]);
			}
			if (dna) dna->setEdgeCount(dna->getEdgeCount() + 1);
		}

		Quad* sideF[4] = {nullptr,nullptr,nullptr,nullptr};
		for (int i=0;i<4;++i) 
		{
			const int i1 = (i+1)&3;
			sideF[i] = makeQuad(
				oldV[i], oldV[i1], nv[i1], nv[i],
				findOrMakeEdge(edges, oldV[i], oldV[i1]),
				findOrMakeEdge(edges, oldV[i1], nv[i1]),
				findOrMakeEdge(edges, nv[i1], nv[i]),
				findOrMakeEdge(edges, nv[i], oldV[i])
			);
			if (mesh) {
				sideF[i]->setParentMesh(mesh);
			}
			faces.push_back(sideF[i]);
			dna->setQuadCount(dna->getQuadCount() + 1);
		}

		Quad* cap = makeQuad(nv[0], nv[1], nv[2], nv[3], capE[0], capE[1], capE[2], capE[3]);
		if (mesh) {
			cap->setParentMesh(mesh);
		}
		faces.push_back(cap);
		dna->setQuadCount(dna->getQuadCount() + 1);

		auto it = std::find(faces.begin(), faces.end(), target);
		if (it != faces.end()) 
		{
			Face* toDelete = *it;
			faces.erase(it);
			if constexpr (has_destroy<Face>::value) 
			{
				toDelete->destroy();
			}
			delete toDelete;
			dna->setQuadCount(dna->getQuadCount() - 1);
		}

		std::vector<Face*> allFaces;
		allFaces.reserve(faces.size());
		for (Face* f : faces) {
			if (f) allFaces.push_back(f);
		}

		for (Edge* edge : edges)
		{
			if (!edge) continue;
			std::vector<Face*> sharedFaces;
			
			Vertice* vertA = edge->getStart();
			Vertice* vertB = edge->getEnd();
			if (!vertA || !vertB) continue;
			
			for (Face* face : allFaces)
			{
				if (!face) continue;
				const auto& faceEdges = face->getEdges();
				
				bool hasEdge = false;
				for (Edge* faceEdge : faceEdges)
				{
					if (faceEdge == edge)
					{
						hasEdge = true;
						break;
					}
				}
				
				if (hasEdge)
				{
					bool alreadyAdded = false;
					for (Face* existingFace : sharedFaces)
					{
						if (existingFace == face)
						{
							alreadyAdded = true;
							break;
						}
					}
					
					if (!alreadyAdded)
					{
						sharedFaces.push_back(face);
						
						if (sharedFaces.size() > 2)
						{
							std::cout << "[ExtrudeFace] WARNING: Edge has more than 2 shared faces! This violates manifold topology." << std::endl;
							sharedFaces.resize(2);
							break;
						}
					}
				}
			}
			
			edge->setSharedFaces(sharedFaces);
		}

		if (out) 
		{
			out->ok = true;
			for (int i=0;i<4;++i) 
			{
				out->newVerts[i] = nv[i];
				out->capEdges[i] = capE[i];
				out->upEdges[i] = upE[i];
				out->sideFaces[i] = sideF[i];
				out->oldVerts[i] = oldV[i];
				out->oldEdges[i] = oldE[i];
			}
			out->capFace  = cap;
			out->distance = distance;
		}   

		return true;
	}



} 