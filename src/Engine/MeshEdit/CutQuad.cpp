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
		std::vector<Edge*> firstQuadEdges;
		std::vector<Edge*> firstRowEdges;
		std::string currentCenterEdgeID = "";

		// First step : Create the center vertices
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

		// Second step : split the edges
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

		// Third step : draw the centers edges to connect the center vertices
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
			
			// close the loop of center edges
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

		std::unordered_set<std::string> centerEdgeIDs;

		// --- color in red the center edges that have no shared faces
		for (Edge* edge : centerEdges)
		{
			if (!edge) continue;
			auto& sharedFaces = edge->getSharedFacesNonConst();
			if (sharedFaces.empty())
			{
				edge->setColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)); 
				centerEdgeIDs.insert(edge->getID());
			}
		}


		// ------- find the other vertice
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
		
		
		glm::vec3 normalDir = glm::normalize(glm::cross(cutDir, refDir));


		// operation fo the current center vertice
		for (int i = 0; i < centers.size(); ++i)
		{
			Edge* currentPerpendiculaireEdge = nullptr;
			Vertice* currentVert = centers[i];			
			if (!currentVert) continue;

			const auto& connectedEdges = currentVert->getEdges();

			for (Edge* edge : connectedEdges)
			{
				if (!edge) continue;

				bool isNotCenterEdge = (centerEdgeIDs.find(edge->getID()) == centerEdgeIDs.end());
				if (isNotCenterEdge)
				{

					Vertice* otherVert = (edge->getStart() == currentVert) ? edge->getEnd() : edge->getStart();
					glm::vec3 edgeDir = glm::normalize(otherVert->getLocalPosition() - currentVert->getLocalPosition());
					float dot = glm::dot(edgeDir, refDir);

					if (dot < 0.0f)
					{
						if(edge->hasbeenMarkedOnceInCutQuad == false)
						{
							edge->setColor(glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)); 
							firstRowEdges.push_back(edge);
						}

						if (i == 0)
						{
							edge->setColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
							edge->hasbeenMarkedOnceInCutQuad = true;
							currentPerpendiculaireEdge = edge;
							firstQuadEdges.push_back(currentPerpendiculaireEdge);							
						}
					}
				}
			}
			
			// here we check for the next vertice and we find the edge connecting current vert and next center vert
			if (i == 0)
			{
				size_t nextIndex = (i + 1) % centers.size();
				Vertice* nextVert = centers[nextIndex];
				Edge* nextPerpendiculaireEdge = nullptr;
				Vertice* nextPerendiculaireEdgeEndVertice = nullptr;

				// -------------- Find the edge perpendicular to the vertice marqued by the next index
				for (Edge* e : nextVert->getEdges())
				{

					if (!e || centerEdgeIDs.find(e->getID()) != centerEdgeIDs.end()) 
					continue;
					
					Vertice* other = (e->getStart() == nextVert) ? e->getEnd() : e->getStart();
					
					if (!other) continue;
						
					glm::vec3 edgeDir = glm::normalize(other->getLocalPosition() - nextVert->getLocalPosition());
					float dot = glm::dot(edgeDir, refDir);

					if (dot < 0.0f)
					{
						e->setColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)); 
						e->hasbeenMarkedOnceInCutQuad = true;
						nextPerpendiculaireEdge = e;
						firstQuadEdges.push_back(nextPerpendiculaireEdge);
						break;
					}

				}

				// ------------------ Find the connected edge
				Edge* connectingEdge = nullptr;

				for (Edge* e : centerEdges)
				{
					if (!e) continue;
					Vertice* start = e->getStart();
					Vertice* end = e->getEnd();

					if ((start == currentVert && end == centers[nextIndex]) || (start == centers[nextIndex] && end == currentVert))
					{
						connectingEdge = e;
						break;
					}
				}

				// for the debug, we color in green the center edge connecting the first and second center vertice
				if (connectingEdge && connectingEdge->hasbeenMarkedOnceInCutQuad == false)
				{
					currentCenterEdgeID = connectingEdge->getID();
					connectingEdge->setColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
					connectingEdge->hasbeenMarkedOnceInCutQuad = true;

					// now we have the second edge of the first quad
					firstQuadEdges.push_back(connectingEdge);

				}
			}
		}

		// ------------- End of the operation for the vertice

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