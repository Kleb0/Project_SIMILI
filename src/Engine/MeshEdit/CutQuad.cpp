#include <glm/vec4.hpp>

// Prototype
void markLonelyEdgesRed(class Mesh* mesh);
#include "Engine/MeshEdit/CutQuad.hpp"
#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "WorldObjects/Basic/Quad.hpp"
#include "WorldObjects/Mesh_DNA/Mesh_DNA.hpp"
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
		std::vector<Vertice*> firstRing;

		// Create the center vertices
		for (Edge* edge : loop) 
		{
			if (!edge || !mesh) continue;
			Vertice* v1 = edge->getStart();
			Vertice* v2 = edge->getEnd();
			if (!v1 || !v2) continue;

			glm::vec3 pos1 = v1->getLocalPosition();
			glm::vec3 pos2 = v2->getLocalPosition();

			glm::vec3 mid = (pos1 + pos2) * 0.5f;
			Vertice* centerVert = mesh->addVertice(mid);

			if (centerVert) 
			{
				centerVert->initialize();
				centers.push_back(centerVert);
			}
		}

		// split the edges
		for (size_t i = 0; i < loop.size() && i < centers.size(); ++i)
		{
			Edge* edge = loop[i];
			Vertice* centerVert = centers[i];
			if (edge && centerVert && mesh)
			{
				Vertice* v1 = edge->getStart();
				Vertice* v2 = edge->getEnd();
				
				edge->splitEdge(centerVert, mesh);
				
				Edge* e1 = nullptr;
				Edge* e2 = nullptr;
				for (Edge* e : mesh->getEdges())
				{
					if (!e) continue;
					if ((e->getStart() == v1 && e->getEnd() == centerVert) || 
						(e->getStart() == centerVert && e->getEnd() == v1))
					{
						e1 = e;
					}
					if ((e->getStart() == centerVert && e->getEnd() == v2) || 
						(e->getStart() == v2 && e->getEnd() == centerVert))
					{
						e2 = e;
					}
				}
				
				if (e1) centerVert->addEdge(e1);
				if (e2) centerVert->addEdge(e2);
				
				if (e1) v1->addEdge(e1);
				if (e2) v2->addEdge(e2);
			}
		}		

		std::vector<Edge*> centerEdges;

		// draw the centers edges to connect the center vertices
		if (mesh && centers.size() > 1) 
		{
			for (size_t i = 0; i < centers.size() - 1; ++i)
			{
				Vertice* vStart = centers[i];
				Vertice* vEnd = centers[i + 1];

				if (vStart && vEnd) 
				{
					Edge* newEdge = mesh->addEdge(vStart, vEnd);

					if (newEdge) 
					{
						newEdge->initialize();

						centerEdges.push_back(newEdge);
						vStart->addEdge(newEdge);
						vEnd->addEdge(newEdge);

					}
				}
			}

			Vertice* vFirst = centers.front();
			Vertice* vLast = centers.back();

			// here i set the color of the first and last center vertices for debug
			// vFirst->setColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
			// vLast->setColor(glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
			
			if (vFirst && vLast) 
			{
				Edge* closingEdge = mesh->addEdge(vLast, vFirst);
				if (closingEdge) 
				{
					closingEdge->initialize();
					centerEdges.push_back(closingEdge);
					vLast->addEdge(closingEdge);
					vFirst->addEdge(closingEdge);
				}
			}
		}

		// std::cout << "\n[cutQuad] Final summary of edges per centerVert :" << std::endl;
		// std::cout << "----------------------------------------" << std::endl;

		// for (size_t i = 0; i < centers.size(); ++i) 
		// {
		// 	Vertice* v = centers[i];
		// 	std::cout << "\n [cutQuad] the vertice at " << i << " with ID " << v->getID() << " has " << v->getEdges().size() << " edges connected to it. IDs : ";
		// 	const auto& edges = v->getEdges();
		// 	for (const auto& e : edges) 
		// 	{
		// 		std::cout << e->getID() << " ";
		// 	}
		// 	std::cout << std::endl;
		// }

		for (Quad* quad : traversedQuads)
		{
			const std::array<Edge*, 4>& quadEdges = quad->getEdgesArray();
			for (Edge* edge : quadEdges)
			{
				if (!edge) continue;
				auto& sharedFaces = edge->getSharedFacesNonConst();
				auto itFace = std::find(sharedFaces.begin(), sharedFaces.end(), quad);
				if (itFace != sharedFaces.end())
				{
					sharedFaces.erase(itFace);
				}
			}
			quad->destroy();
		}

		// e->setColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)); // rouge
		// ---------------- 
		

		// here we create the faces 


		// glm::vec3 startPos = start->getLocalPosition();
		// glm::vec3 endPos = end->getLocalPosition();
		// glm::vec3 dir = glm::normalize(endPos - startPos);
		// edge->setColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)); // rouge

		// 1 - On colore en rouge les edges qui s'arrêtent au centerVert
		//     mais uniquement une moitié sur deux (alternance par centerVert)
		// -2 ils s'arrêtent au centered vertice qu'on a positionné 
		// -3 les edges reliant les centered vertices n'ont aucune shared face. *
		// - 4 Pour le test on va set en rouge nos edges qui forment l'ossature des futurs quads


		// // ------------------------------
		// // Update MeshDNA quad count
		// if (mesh && mesh->getMeshDNA())
		// {
		// 	MeshDNA* mdna = mesh->getMeshDNA();
		// 	int currentQuadCount = mesh->getQuads().size();
			
		// 	std::cout << "[CutQuad] Updated MeshDNA quad count to: " << currentQuadCount << std::endl;
		// 	mdna->setQuadCount(currentQuadCount);	
		// }
	}
}