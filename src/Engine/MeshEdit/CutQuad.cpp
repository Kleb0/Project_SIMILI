#include "Engine/MeshEdit/CutQuad.hpp"
#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <algorithm>
#include "Engine/ErrorBox.hpp"

namespace MeshEdit 
{
	void CutQuad(const std::vector<Edge*>& loop, Mesh* mesh, const std::vector<Quad*>& traversedQuads)
	{
		std::vector<Vertice*> centers;
		for (Edge* edge : loop)
		{
			if (!edge) continue;
			Vertice* vA = edge->getStart();
			Vertice* vB = edge->getEnd();
			if (!vA || !vB) continue;
			glm::vec3 center = 0.5f * (vA->getLocalPosition() + vB->getLocalPosition());
			Vertice* vCenter = mesh->addVertice(center);
			centers.push_back(vCenter);
		}

		bool referenceSet = false;
		glm::vec3 refDirA, refDirB;
		
		for (size_t i = 0; i < traversedQuads.size(); ++i)
		{
			Quad* quad = traversedQuads[i];
			if (!quad) continue;

			size_t entryEdgeIdx = i;
			size_t exitEdgeIdx = (i + 1) % loop.size();
			
			Edge* entryEdge = loop[entryEdgeIdx];
			Edge* exitEdge = loop[exitEdgeIdx];

			const auto& quadEdges = quad->getEdgesArray();
			bool entryFound = false, exitFound = false;
			for (int k = 0; k < 4; ++k)
			{
				if (quadEdges[k] == entryEdge) entryFound = true;
				if (quadEdges[k] == exitEdge) exitFound = true;
			}
			
			if (!entryFound || !exitFound) 
			{
				std::cout << "Erreur: edges du loop non trouvés dans le quad " << i << std::endl;
				continue;
			}

			Vertice* a1, *a2, *a3;
			a2 = centers[entryEdgeIdx];
			
			if (i == 0) {
				a1 = entryEdge->getStart();
				a3 = entryEdge->getEnd();
				refDirA = glm::normalize(a3->getLocalPosition() - a1->getLocalPosition());
				referenceSet = true;
			} else {
				glm::vec3 currentDirA = entryEdge->getEnd()->getLocalPosition() - entryEdge->getStart()->getLocalPosition();
				if (glm::dot(glm::normalize(currentDirA), refDirA) > 0) {
					a1 = entryEdge->getStart();
					a3 = entryEdge->getEnd();
				} else {
					a1 = entryEdge->getEnd();
					a3 = entryEdge->getStart();
				}
			}
			
			std::array<Vertice*, 3> vertsA = {a1, a2, a3};

			Vertice* b1, *b2, *b3;
			b2 = centers[exitEdgeIdx];
			
			if (i == 0) {
				glm::vec3 a3_pos = a3->getLocalPosition();
				Vertice* b1_option1 = exitEdge->getStart();
				Vertice* b1_option2 = exitEdge->getEnd();
				
				float dist1 = glm::length(a3_pos - b1_option1->getLocalPosition());
				float dist2 = glm::length(a3_pos - b1_option2->getLocalPosition());
				
				if (dist1 > dist2) {
					b1 = b1_option1;
					b3 = exitEdge->getEnd();
				} else {
					b1 = b1_option2;
					b3 = exitEdge->getStart();
				}
				
				refDirB = glm::normalize(b3->getLocalPosition() - b1->getLocalPosition());
			} else {
				glm::vec3 currentDirB = exitEdge->getEnd()->getLocalPosition() - exitEdge->getStart()->getLocalPosition();
				if (glm::dot(glm::normalize(currentDirB), refDirB) > 0) {
					b1 = exitEdge->getStart();
					b3 = exitEdge->getEnd();
				} else {
					b1 = exitEdge->getEnd();
					b3 = exitEdge->getStart();
				}
			}
			
			std::array<Vertice*, 3> vertsB = {b1, b2, b3};

			quad->setCutVertices(vertsA, vertsB);

			std::cout << "\n=== DEBUG QUAD " << i << " ===" << std::endl;
			std::cout << "Entry Edge (A): " << entryEdgeIdx << std::endl;
			std::cout << "Exit Edge (B): " << exitEdgeIdx << std::endl;
			std::cout << "A1: (" << a1->getLocalPosition().x << ", " << a1->getLocalPosition().y << ", " << a1->getLocalPosition().z << ")" << std::endl;
			std::cout << "A2: (" << a2->getLocalPosition().x << ", " << a2->getLocalPosition().y << ", " << a2->getLocalPosition().z << ")" << std::endl;
			std::cout << "A3: (" << a3->getLocalPosition().x << ", " << a3->getLocalPosition().y << ", " << a3->getLocalPosition().z << ")" << std::endl;
			std::cout << "B1: (" << b1->getLocalPosition().x << ", " << b1->getLocalPosition().y << ", " << b1->getLocalPosition().z << ")" << std::endl;
			std::cout << "B2: (" << b2->getLocalPosition().x << ", " << b2->getLocalPosition().y << ", " << b2->getLocalPosition().z << ")" << std::endl;
			std::cout << "B3: (" << b3->getLocalPosition().x << ", " << b3->getLocalPosition().y << ", " << b3->getLocalPosition().z << ")" << std::endl;

			quad->splitQuadFromCut(mesh);

			auto& faces = mesh->getFacesNonConst();
			auto it = std::find(faces.begin(), faces.end(), quad);
			if (it != faces.end())
			{
				faces.erase(it);
			}
			quad->destroy();
			delete quad;
		}

		for (size_t i = 0; i < loop.size(); ++i)
		{
			Edge* edge = loop[i];
			Vertice* centerVertex = centers[i];
			if (edge && centerVertex)
			{
				edge->splitEdge(centerVertex, mesh);
			}
		}

		mesh->updateGeometry();
	}
}
