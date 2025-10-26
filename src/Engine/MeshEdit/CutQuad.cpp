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
		std::vector<Edge*> QuadEdges;
		std::vector<Edge*> firstRowEdges;

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
				centerEdgeIDs.insert(edge->getID());
			}
		}


		findFirstQuadEdgesWithVertice(centers, centerEdgeIDs, centerEdges, mesh, firstRowEdges, QuadEdges);

		// Clean up : remove the traversed quads from the mesh and from the edges shared faces
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
		}
		
		// Retire the quads from the mesh
		if (mesh)
		{
			auto& meshFaces = mesh->getFacesNonConst();
			for (Quad* quad : traversedQuads)
			{
				auto it = std::find(meshFaces.begin(), meshFaces.end(), quad);
				if (it != meshFaces.end())
				{
					meshFaces.erase(it);
				}
				quad->destroy();
				delete quad;
			}
		}

		// ----------------------------

	
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

	void findFirstQuadEdgesWithVertice(const std::vector<Vertice*>& centers, const std::unordered_set<std::string>& centerEdgeIDs, 
	const std::vector<Edge*>& centerEdges, Mesh* mesh, std::vector<Edge*>& firstRowEdges, std::vector<Edge*>& QuadEdges)
	{
		// ------- find the next vertice
		glm::vec3 cutDir = glm::normalize(centers.back()->getLocalPosition() - centers.front()->getLocalPosition());		
		glm::vec3 refDir = glm::vec3(0.0f);

		for (Vertice* cv : centers)
		{
			if (!cv) continue;

			for (Edge* e : cv->getEdges())
			{
				if (!e || centerEdgeIDs.find(e->getID()) != centerEdgeIDs.end()) continue;

				Vertice* next = (e->getStart() == cv) ? e->getEnd() : e->getStart();

				if (next)
				{
					refDir = glm::normalize(next->getLocalPosition() - cv->getLocalPosition());
					break;
				}
			}
			if (glm::length(refDir) > 0.01f) break;
		}
		
		
		glm::vec3 normalDir = glm::normalize(glm::cross(cutDir, refDir));

		// operation fo the current center vertice, the the current vert will be the departure point to build the quad
		for (int i = 0; i < centers.size(); ++i)
		{
			Edge* currentPerpendicularEdge = nullptr;
			Vertice* currentVert = centers[i];			
			if (!currentVert) continue;

			const auto& connectedEdges = currentVert->getEdges();

			for (Edge* edge : connectedEdges)
			{
				if (!edge) continue;


				// i decided to start finding the perpendicular edges using centerEdgs as reference
				bool isNotCenterEdge = (centerEdgeIDs.find(edge->getID()) == centerEdgeIDs.end());
				if (isNotCenterEdge)
				{

					// the vector 3 edgeDir give us the the direction of the edge connected to the current center vertice
					Vertice* otherVert = (edge->getStart() == currentVert) ? edge->getEnd() : edge->getStart();
					glm::vec3 edgeDir = glm::normalize(otherVert->getLocalPosition() - currentVert->getLocalPosition());
					float dot = glm::dot(edgeDir, refDir);

					// i decided to start from the part that inferior to zero, remember we have found the perpendicular edges to the cut direction
					if (dot < 0.0f)
					{
						if(edge->hasbeenMarkedOnceInCutQuad == false)
						{
							// edge->setColor(glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)); 
							firstRowEdges.push_back(edge);
						}
						
						// ---- Departure point to build the quad
						// ----------- here we find the first edge of the quad
						if (i == 0)
						{
							// edge->setColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
							edge->hasbeenMarkedOnceInCutQuad = true;
							currentPerpendicularEdge = edge;
							QuadEdges.push_back(currentPerpendicularEdge);							
						}
					}
				}
			}
			
		
			// here we check for the next vertice and we find the edge connecting current vert and next center vert
			if (i == 0)
			{

				// ---------- 1 : find the next vertice
				size_t nextIndex = (i + 1) % centers.size();
				Vertice* nextVert = centers[nextIndex];
				Edge* nextPerpendicularEdge = nullptr;
				Vertice* nextPerpendicularEdgeEndVertice = nullptr;

				// ------------------ 2 Find the connected edge witch is also a center edge, the second edge
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
					// connectingEdge->setColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
					connectingEdge->hasbeenMarkedOnceInCutQuad = true;

					// now we have the second edge of the first quad
					QuadEdges.push_back(connectingEdge);

				}

				// -------------- 3 : Find the  perpendicular edge to the vertice marqued by the next index
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
						// e->setColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)); 
						e->hasbeenMarkedOnceInCutQuad = true;
						nextPerpendicularEdge = e;
						QuadEdges.push_back(nextPerpendicularEdge);
						break;
					}
				}

				//--------- 4 : find the fourth edge of the quad
				if(currentPerpendicularEdge != nullptr && nextPerpendicularEdge != nullptr)
				{

					Vertice* vA = (currentPerpendicularEdge->getStart() == currentVert) ? currentPerpendicularEdge->getEnd() : currentPerpendicularEdge->getStart();
					Vertice* vB = (nextPerpendicularEdge->getStart() == nextVert) ? nextPerpendicularEdge->getEnd() : nextPerpendicularEdge->getStart();

					Edge* fourthEdge = nullptr;

					for (Edge* e : vA->getEdges()) 
					{
						if (!e || centerEdgeIDs.find(e->getID()) != centerEdgeIDs.end()) continue;
						if ((e->getStart() == vA && e->getEnd() == vB) || (e->getStart() == vB && e->getEnd() == vA)) {
							fourthEdge = e;
							break;
						}
					}

					if (fourthEdge) 
					{
						// fourthEdge->setColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
						fourthEdge->hasbeenMarkedOnceInCutQuad = true;
						QuadEdges.push_back(fourthEdge);
					}
				}

				// -------- create the quad
				buildQuadFromEdges(QuadEdges, currentVert, nextVert, mesh);

				std::cout << "[buildFirstQuadfromVertice] Cleaning hasbeenMarkedOnceInCutQuad flags..." << std::endl;
				for (Edge* e : mesh->getEdges())
				{
					if (e) 
					{
						e->hasbeenMarkedOnceInCutQuad = false;
					}
				}

				// -------- Now try to find the edges for the second quad (index 1)
				findNextQuadEdgesForFirstRow(centers, centerEdgeIDs, centerEdges, mesh, QuadEdges, refDir, 1);

			}
		}

		// ------------- End of the operation for the vertice
	}

	void findNextQuadEdgesForFirstRow(const std::vector<Vertice*>& centers, const std::unordered_set<std::string>& centerEdgeIDs, 
	const std::vector<Edge*>& centerEdges, Mesh* mesh, std::vector<Edge*>& QuadEdges, const glm::vec3& refDir, int currentIndex)
	{
		QuadEdges.clear();

		if (currentIndex < 0 || currentIndex >= static_cast<int>(centers.size())) 
		{
			std::cout << "[findNextQuadEdgesForFirstRow] Invalid currentIndex: " << currentIndex << std::endl;
			return;
		}

		Vertice* currentVert = centers[currentIndex];
		size_t nextIndex = (currentIndex + 1) % centers.size();
		Vertice* nextVert = centers[nextIndex];

		if (!currentVert || !nextVert) 
		{
			std::cout << "[findNextQuadEdgesForFirstRow] Invalid vertices" << std::endl;
			return;
		}

		std::cout << "[findNextQuadEdgesForFirstRow] Processing quad between center vertex " 
				  << currentIndex << " and " << nextIndex << std::endl;

		// ---------- 1: Find the first perpendicular edge (connected to currentVert)
		// This edge can already be marked because it's shared with the previous quad
		Edge* currentPerpendicularEdge = nullptr;
		const auto& connectedEdges = currentVert->getEdges();

		for (Edge* edge : connectedEdges)
		{
			if (!edge) continue;

			// Skip if it's a center edge
			bool isNotCenterEdge = (centerEdgeIDs.find(edge->getID()) == centerEdgeIDs.end());
			if (!isNotCenterEdge) continue;

			Vertice* otherVert = (edge->getStart() == currentVert) ? edge->getEnd() : edge->getStart();
			if (!otherVert) continue;

			glm::vec3 edgeDir = glm::normalize(otherVert->getLocalPosition() - currentVert->getLocalPosition());
			float dot = glm::dot(edgeDir, refDir);

			if (dot < 0.0f)
			{
				// Accept this edge even if already marked (it's shared with previous quad)
				edge->setColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green for debug
				currentPerpendicularEdge = edge;
				QuadEdges.push_back(currentPerpendicularEdge);
				std::cout << "[findNextQuadEdgesForFirstRow] Found first perpendicular edge (green) - already marked: " 
						  << (edge->hasbeenMarkedOnceInCutQuad ? "YES" : "NO") << std::endl;
				break;
			}
		}

		if (!currentPerpendicularEdge)
		{
			std::cout << "[findNextQuadEdgesForFirstRow] Could not find current perpendicular edge" << std::endl;
			return;
		}

		// ---------- 2: Find the connecting center edge
		Edge* connectingEdge = nullptr;
		for (Edge* e : centerEdges)
		{
			if (!e) continue;
			Vertice* start = e->getStart();
			Vertice* end = e->getEnd();

			if ((start == currentVert && end == nextVert) || (start == nextVert && end == currentVert))
			{
				connectingEdge = e;
				break;
			}
		}

		if (connectingEdge)
		{
			connectingEdge->setColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green for debug
			if (connectingEdge->hasbeenMarkedOnceInCutQuad == false)
			{
				connectingEdge->hasbeenMarkedOnceInCutQuad = true;
			}
			QuadEdges.push_back(connectingEdge);
			std::cout << "[findNextQuadEdgesForFirstRow] Found connecting center edge (green)" << std::endl;
		}
		else
		{
			std::cout << "[findNextQuadEdgesForFirstRow] Could not find connecting edge" << std::endl;
			return;
		}

		// ---------- 3: Find the perpendicular edge on the next vertex
		Edge* nextPerpendicularEdge = nullptr;
		for (Edge* e : nextVert->getEdges())
		{
			if (!e || centerEdgeIDs.find(e->getID()) != centerEdgeIDs.end()) 
				continue;
			
			Vertice* other = (e->getStart() == nextVert) ? e->getEnd() : e->getStart();
			if (!other) continue;
				
			glm::vec3 edgeDir = glm::normalize(other->getLocalPosition() - nextVert->getLocalPosition());
			float dot = glm::dot(edgeDir, refDir);

			// We stay in the same direction (dot < 0.0f)
			if (dot < 0.0f)
			{
				if (e->hasbeenMarkedOnceInCutQuad == false)
				{
					e->setColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green for debug
					e->hasbeenMarkedOnceInCutQuad = true;
					nextPerpendicularEdge = e;
					QuadEdges.push_back(nextPerpendicularEdge);
					std::cout << "[findNextQuadEdgesForFirstRow] Found next perpendicular edge (green)" << std::endl;
					break;
				}
			}
		}

		if (!nextPerpendicularEdge)
		{
			std::cout << "[findNextQuadEdgesForFirstRow] Could not find next perpendicular edge" << std::endl;
			return;
		}

		// ---------- 4: Find the fourth edge (parallel to the center edge)
		Vertice* vA = (currentPerpendicularEdge->getStart() == currentVert) ? currentPerpendicularEdge->getEnd() : currentPerpendicularEdge->getStart();
		Vertice* vB = (nextPerpendicularEdge->getStart() == nextVert) ? nextPerpendicularEdge->getEnd() : nextPerpendicularEdge->getStart();

		Edge* fourthEdge = nullptr;
		for (Edge* e : vA->getEdges()) 
		{
			if (!e || centerEdgeIDs.find(e->getID()) != centerEdgeIDs.end()) continue;
			
			if ((e->getStart() == vA && e->getEnd() == vB) || (e->getStart() == vB && e->getEnd() == vA)) 
			{
				fourthEdge = e;
				break;
			}
		}

		if (fourthEdge) 
		{
			fourthEdge->setColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)); // Green for debug
			fourthEdge->hasbeenMarkedOnceInCutQuad = true;
			QuadEdges.push_back(fourthEdge);
		}
		else
		{
			return;
		}
	}

	void buildQuadFromEdges(const std::vector<Edge*>& QuadEdges, Vertice* currentVert, Vertice* nextVert, Mesh* mesh)
	{
		if (QuadEdges.empty())
		{
			return;
		}

		if (QuadEdges.size() != 4) 
		{
			return;
		}

		if (!currentVert || !nextVert || !mesh)
		{
			return;
		}

		std::array<Edge*, 4> quadEdges;
		std::array<Vertice*, 4> quadVertices;
		
		// for now, i decided to assume arbitrarily the order of the edges in QuadEdges
		Edge* perpEdge1 = QuadEdges[0];
		Edge* centerEdge = QuadEdges[1];
		Edge* perpEdge2 = QuadEdges[2];
		Edge* parallelEdge = QuadEdges[3];
		
		Vertice* v0 = currentVert;
		Vertice* v3 = nextVert;
		
		Vertice* v1 = (perpEdge1->getStart() == v0) ? perpEdge1->getEnd() : perpEdge1->getStart();
		Vertice* v2 = (perpEdge2->getStart() == v3) ? perpEdge2->getEnd() : perpEdge2->getStart();
		
		quadVertices[0] = v0;
		quadVertices[1] = v1;
		quadVertices[2] = v2;
		quadVertices[3] = v3;
		
		quadEdges[0] = perpEdge1;
		quadEdges[1] = parallelEdge;
		quadEdges[2] = perpEdge2; 
		quadEdges[3] = centerEdge;
		
		Quad* newQuad = mesh->addQuad(quadVertices, quadEdges);
		
		if (newQuad)
		{
			newQuad->initialize();
			
			for (int k = 0; k < 4; ++k) 
			{
				auto& sharedFaces = quadEdges[k]->getSharedFacesNonConst();
				sharedFaces.push_back(newQuad);
			}
			
			newQuad->setParentMesh(mesh);							
		}
		else
		{
		}
	}


}