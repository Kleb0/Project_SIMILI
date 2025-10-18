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
				edge->splitEdge(centerVert, mesh);
                centerVert->addEdge(edge);
                Vertice* v1 = edge->getStart();
                Vertice* v2 = edge->getEnd();
                v1->addEdge(edge);
               	v2->addEdge(edge);
			}
		}

		std::vector<Edge*> centerEdges;

		// draw the centers to connect the center vertices
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

						std::cout << "\n [CutQuad] Created center edge " << i << " connecting centers" << std::endl;
						std::cout << "[cutQuad]  from center vert ID: " << vStart->getID() << " to center vert ID: " << vEnd->getID() << std::endl;
						std::cout << "[cutQuad] the vertice at " << i << " with ID " << vStart->getID() << " has " << vStart->getEdges().size() << " edges connected to it." << std::endl;

					}
				}
			}

			Vertice* vFirst = centers.front();

			// vLast is the not the first center and has 3 edges (meaning it's not connect yet to the first center)
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

		// ---------------- 
		if (mesh) 
		{
			const auto& edges = mesh->getEdges();
			for (Edge* edge : edges) 
			{
				if (!edge) continue;
				// On ne s'intéresse qu'aux edges qui ont au moins une sharedFace
				if (!edge->getSharedFaces().empty()) 
				{
					// Pour chaque sommet de cet edge, on regarde ses edges sortants
					Vertice* v1 = edge->getStart();
					Vertice* v2 = edge->getEnd();

					if (v1) 
					{
						for (Edge* e : v1->getEdges()) 
						{
							if (e && e->getSharedFaces().empty()) {
								e->setColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)); // rouge
							}
						}
					}
					if (v2) 
					{
						for (Edge* e : v2->getEdges()) 
						{
							if (e && e->getSharedFaces().empty()) 
							{
								e->setColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)); // rouge
							}
						}
					}
				}
			}
		}

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