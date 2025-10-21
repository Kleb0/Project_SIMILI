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

		// ---- colorier en rouge les edges dont l'adjacent face est de 0

		for (Edge* edge : centerEdges)
		{
			if (!edge) continue;
			auto& sharedFaces = edge->getSharedFacesNonConst();
			if (sharedFaces.empty())
			{
				edge->setColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)); 
			}
		}

		// ---- First, set in a list the IDs of center edges
		std::unordered_set<std::string> centerEdgeIDs;
		for (Edge* edge : centerEdges)
		{
			if (edge) centerEdgeIDs.insert(edge->getID());
		}


		glm::vec3 cutDir = glm::normalize(centers.back()->getLocalPosition() - centers.front()->getLocalPosition());
		
		glm::vec3 refDir = glm::vec3(0.0f);
		for (Vertice* cv : centers)
		{
			if (!cv) continue;

			for (Edge* e : cv->getEdges())
			{
				if (!e || centerEdgeIDs.find(e->getID()) != centerEdgeIDs.end()) continue;

				Vertice* other = (e->getStart() == cv) ? e->getEnd() : e->getStart();

				if (other)
				{
					refDir = glm::normalize(other->getLocalPosition() - cv->getLocalPosition());
					break;
				}
			}
			if (glm::length(refDir) > 0.01f) break;
		}
		
		// Normal perpendicular au cut pour séparer les deux côtés
		glm::vec3 normalDir = glm::normalize(glm::cross(cutDir, refDir));

		std::cout << "\n ---- [CutQuad] ---- Directions used for coloring edges :" << std::endl;
		std::cout << "[CutQuad] cutDir: (" << cutDir.x << ", " << cutDir.y << ", " << cutDir.z << ")" << std::endl;
		std::cout << "[CutQuad] refDir: (" << refDir.x << ", " << refDir.y << ", " << refDir.z << ")" << std::endl;

		for (Vertice* centerVert : centers)
		{
			if (!centerVert) continue;

			// in a loop, edge vertice created has 4 edges, we need to filter them
			const auto& connectedEdges = centerVert->getEdges();

			for (Edge* edge : connectedEdges)
			{
				if (!edge) continue;

				if (centerEdgeIDs.find(edge->getID()) != centerEdgeIDs.end())
				{
					// these are the center edges 
					edge->setColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)); 
				}
				else
				{

					Vertice* otherVert = (edge->getStart() == centerVert) ? edge->getEnd() : edge->getStart();
					glm::vec3 edgeDir = glm::normalize(otherVert->getLocalPosition() - centerVert->getLocalPosition());
					float dot = glm::dot(edgeDir, refDir);

					std::cout << "[CutQuad] Edge ID: " << edge->getID() << " dot : " << dot << " | this edge can't be colored in blue " << std::endl;

					if (dot < 0.0f)
					{
						edge->setColor(glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)); 
						std::cout << "[CutQuad] -> Colored BLUE" << std::endl;

						// ---- here we create the faces

					}
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