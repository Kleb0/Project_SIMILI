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
		std::vector<Edge*> traversedEdges;
		
		std::vector<std::array<Vertice*, 3>> allVertsA;
		std::vector<std::array<Vertice*, 3>> allVertsB;

		for (Edge* edge : loop)
		{
			if (!edge) continue;
			Vertice* vA = edge->getStart();
			Vertice* vB = edge->getEnd();
			if (!vA || !vB) continue;
			glm::vec3 center = 0.5f * (vA->getLocalPosition() + vB->getLocalPosition());
			Vertice* vCenter = mesh->addVertice(center);
			vCenter->initialize();
			centers.push_back(vCenter);
			traversedEdges.push_back(edge);
		}

		bool referenceSet = false;
		glm::vec3 refDirA, refDirB;

		auto findOrCreateEdge = [&](std::vector<Edge*>& edges, Vertice* a, Vertice* b) -> Edge*
		{
			for (Edge* e : edges)
			{
				if ((e->getStart() == a && e->getEnd() == b) || (e->getStart() == b && e->getEnd() == a)) {
					return e;
				}
			}
			Edge* e = new Edge(a, b);
			e->initialize();
			edges.push_back(e);
			return e;
		};
    
		for (size_t i = 0; i < traversedQuads.size(); ++i)
		{
			//traversedQuads is a list passed by reference in EdgeLoop
			Quad* quad = traversedQuads[i];
			if (!quad) continue;

			//each quad share an entryEdge and an exit edge. The entryEdge of first is the exitEdge of the last quad
			//for each quads in traversedquad, each quad is associate to an edge in the loop.
			//the exit edge is necessarely the index +1, and we maintain the coherence through the loop. By using a modulo on the
			//loop size, we keep staying on the current quad, and when we come back to the first quad, the EdgeEntry became the last 
			//ExitEdge. 

			size_t entryEdgeIdx = i;
			size_t exitEdgeIdx = (i + 1) % loop.size();
        
			Edge* entryEdge = loop[entryEdgeIdx];
			Edge* exitEdge = loop[exitEdgeIdx];

			const auto& quadEdges = quad->getEdgesArray();
			bool entryFound = false, exitFound = false;

			//as a quad has 4 edges, and as the edgeloop is only for quads, we can limit the check loop to 4
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
        
			//the first quad set a norm in order to place a1 and a3 in the right order, a1 is the start of the edge, a3 the end
			// we made some errors by using positions inside the mesh matrix, so we use local positions of the quad itself as standard
			if (i == 0) 
			{
				a1 = entryEdge->getStart();
				a3 = entryEdge->getEnd();
				refDirA = glm::normalize(a3->getLocalPosition() - a1->getLocalPosition());
				referenceSet = true;
			} 
			else 
			{
				glm::vec3 currentDirA = entryEdge->getEnd()->getLocalPosition() - entryEdge->getStart()->getLocalPosition();
				if (glm::dot(glm::normalize(currentDirA), refDirA) > 0) 
				{
					a1 = entryEdge->getStart();
					a3 = entryEdge->getEnd();
				} 
				else 
				{
					a1 = entryEdge->getEnd();
					a3 = entryEdge->getStart();
				}
			}
        
			std::array<Vertice*, 3> vertsA = {a1, a2, a3};

			Vertice* b1, *b2, *b3;
			b2 = centers[exitEdgeIdx];
        
			//use the first quad as a reference for the other quads
			if (i == 0) 
			{
				//without reference, the order of vertices can be wrong from one quad to another as the edges are shared by quads
				glm::vec3 a3_pos = a3->getLocalPosition();
				Vertice* b1_option1 = exitEdge->getStart();
				Vertice* b1_option2 = exitEdge->getEnd();
            
				float dist1 = glm::length(a3_pos - b1_option1->getLocalPosition());
				float dist2 = glm::length(a3_pos - b1_option2->getLocalPosition());
            
				if (dist1 > dist2) 
				{
					b1 = b1_option1;
					b3 = exitEdge->getEnd();
				} 
				else 
				{
					b1 = b1_option2;
					b3 = exitEdge->getStart();
				}
				// calculate the reference direction 
            
				refDirB = glm::normalize(b3->getLocalPosition() - b1->getLocalPosition());
			} 
			else 
			{
				glm::vec3 currentDirB = exitEdge->getEnd()->getLocalPosition() - exitEdge->getStart()->getLocalPosition();
				if (glm::dot(glm::normalize(currentDirB), refDirB) > 0) 
				{
					b1 = exitEdge->getStart();
					b3 = exitEdge->getEnd();
				} 
				else 
				{
					b1 = exitEdge->getEnd();
					b3 = exitEdge->getStart();
				}
			}
        
			std::array<Vertice*, 3> vertsB = {b1, b2, b3};

			quad->setCutVertices(vertsA, vertsB);
			allVertsA.push_back(vertsA);
			allVertsB.push_back(vertsB);
		}

		auto& meshEdges = mesh->getEdgesNonConst();

		for (size_t i = 0; i < loop.size(); ++i)
		{
			Edge* originalEdge = loop[i];
			Vertice* centerVertex = centers[i];

			if (!originalEdge || !centerVertex) continue;

			Vertice* start = originalEdge->getStart();
			Vertice* end = originalEdge->getEnd();
			if (!start || !end) continue;

			findOrCreateEdge(meshEdges, start, centerVertex);
			findOrCreateEdge(meshEdges, centerVertex, end);
		}

		for (size_t i = 0; i < traversedQuads.size(); ++i)
		{
			Quad* quad = traversedQuads[i];
			if (!quad) continue;

			const auto& vertsA = allVertsA[i];
			const auto& vertsB = allVertsB[i];

			Vertice* a2 = vertsA[1];
			Vertice* a3 = vertsA[2];
			Vertice* b2 = vertsB[1];
			Vertice* b3 = vertsB[2];

			Edge* edgeA2A3 = findOrCreateEdge(meshEdges, a2, a3);
			Edge* edgeA3B3 = findOrCreateEdge(meshEdges, a3, b3);
			Edge* edgeB3B2 = findOrCreateEdge(meshEdges, b3, b2);
			Edge* edgeB2A2 = findOrCreateEdge(meshEdges, b2, a2);

			std::array<Vertice*, 4> quadVerts = {a2, a3, b3, b2};
			std::array<Edge*, 4> quadEdges = {edgeA2A3, edgeA3B3, edgeB3B2, edgeB2A2};
			
			Quad* newQuad = new Quad(quadVerts, quadEdges);
			newQuad->initialize();
			mesh->getFacesNonConst().push_back(newQuad);

			quad->splitQuadFromCut(mesh);
			std::cout << "\n  Create Quad" << std::endl;
			
			auto& faces = mesh->getFacesNonConst();
			
			auto it = std::find(faces.begin(), faces.end(), quad);
			if (it != faces.end())
			{
				faces.erase(it);
			}

			quad->destroy();
			delete quad;
		}

		for (Edge* edge : traversedEdges)
		{
			auto& meshEdges = mesh->getEdgesNonConst();
			auto it = std::find(meshEdges.begin(), meshEdges.end(), edge);
			if (it != meshEdges.end())
			{
				(*it)->destroy();
				delete *it;
				meshEdges.erase(it);
			}
		}

	}

}