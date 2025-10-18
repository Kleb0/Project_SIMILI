#include "Engine/MeshEdit/EdgeLoop.hpp"
#include "Engine/MeshEdit/CutQuad.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include "WorldObjects/Basic/Edge.hpp"
#include "WorldObjects/Basic/Quad.hpp"
#include "WorldObjects/Basic/Vertice.hpp"
#include "WorldObjects/Mesh_DNA/Mesh_DNA.hpp"
#include "Engine/ThreeDScene.hpp"
#include <vector>
#include <unordered_set>
#include <imgui.h>

namespace MeshEdit 
{
	
	std::vector<Edge*> FindLoop(Vertice* startVert, Edge* selectedEdge, Mesh* mesh, ThreeDScene* scene, const ImVec2& oglChildPos, const ImVec2& oglChildSize)		
	{

		if(selectedEdge == nullptr)
			return {};


		std::vector<Edge*> DirectionA;
		std::vector<Edge*> DirectionB;		
		std::vector<Edge*> loop;

		std::vector<Quad*> JoiningQuad;
		std::vector<Quad*> visitedQuads;

		static Edge* lastSelectedEdge = nullptr;
		static std::unordered_set<Quad*> alreadyPrinted;
		static bool prevCtrlLeft = false;
		static bool hasJoinedQuad = false;
		
		bool ctrlLeftPressed = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
		bool ctrlLeftJustPressed = ctrlLeftPressed && !prevCtrlLeft;
		prevCtrlLeft = ctrlLeftPressed;
		
		if (lastSelectedEdge != selectedEdge || ctrlLeftJustPressed) 
		{
			alreadyPrinted.clear();
			hasJoinedQuad = false;  // Reset the static bool when edge changes
			lastSelectedEdge = selectedEdge;
		}


		// ---- selected edge part ---- //
		const auto& sharedFaces = selectedEdge->getSharedFaces();
		std::vector<std::vector<Edge*>> directions;

		std::vector<Edge*> startEdges;
		std::vector<Edge*> exitEdges;

		for (Face* f : sharedFaces) 
		{
			Quad* quad = dynamic_cast<Quad*>(f);
			if (!quad) continue;
			Edge* opposite = nullptr;
			for (Edge* e : quad->getEdgesArray()) 
			{
				if (e == selectedEdge) continue;

				Vertice* eStart = e->getStart();
				Vertice* eEnd = e->getEnd();
				Vertice* selStart = selectedEdge->getStart();
				Vertice* selEnd = selectedEdge->getEnd();

				if ((eStart != selStart && eStart != selEnd) && (eEnd != selStart && eEnd != selEnd)) 
				{
					opposite = e;
					break;
				}
			}

			startEdges.push_back(selectedEdge);

			if (opposite) exitEdges.push_back(opposite);

			std::vector<Edge*> direction;
			direction.push_back(selectedEdge);

			if (opposite) direction.push_back(opposite);

			directions.push_back(direction);
			visitedQuads.push_back(quad);
		}
		
		DirectionA.clear();
		DirectionB.clear();
		DirectionA.push_back(selectedEdge);
		DirectionB.push_back(selectedEdge);
		loop.push_back(selectedEdge);
		
		// ----- Determine the two directions as selected edge is shared by two quads ---- //
		if (exitEdges.size() >= 1) 
		{
			Edge* currentA = exitEdges[0];
			Edge* currentB = exitEdges.size() >= 2 ? exitEdges[1] : nullptr;

			std::unordered_set<Quad*> visitedA;
			std::unordered_set<Quad*> visitedB;
			int maxSteps = 20; 
			
			for (int step = 0; step < maxSteps; ++step) 
			{
				bool continueA = false;
				bool continueB = false;
				
				// ---- Direction A ---- //
				if (currentA) 
				{
					DirectionA.push_back(currentA);

					
					const auto& sharedFacesA = currentA->getSharedFaces();
					Edge* nextA = nullptr;

					for (Face* f : sharedFacesA) 
					{
						Quad* quad = dynamic_cast<Quad*>(f);
						if (!quad) continue;

						if (std::find(visitedQuads.begin(), visitedQuads.end(), quad) == visitedQuads.end() &&
							visitedA.find(quad) == visitedA.end()) 
						{
							visitedA.insert(quad);

							bool idAlreadyPresent = false;

							for (Quad* qTest : visitedQuads) 
							{
								if (qTest && qTest->getID() == quad->getID()) 
								{
										idAlreadyPresent = true;
										break;
								}
							}
							if (!idAlreadyPresent) 
							{
								visitedQuads.push_back(quad);
							}

							
							for (Edge* e : quad->getEdgesArray()) 
							{
								if (e == currentA) continue;

								Vertice* eStart = e->getStart();
								Vertice* eEnd = e->getEnd();
								Vertice* currStart = currentA->getStart();
								Vertice* currEnd = currentA->getEnd();

								if ((eStart != currStart && eStart != currEnd) && (eEnd != currStart && eEnd != currEnd)) 
								{
									nextA = e;
									break;
								}
							}
							break;
						}
					}
					
					if (nextA && currentB && std::find(DirectionB.begin(), DirectionB.end(), nextA) != DirectionB.end()) 
					{
						break;
					}
					
					if (nextA) 
					{
						currentA = nextA;
						continueA = true;
					}
				}
				
				// ---- Direction B ---- //

				if (currentB) 
				{
					DirectionB.push_back(currentB);
					
					const auto& sharedFacesB = currentB->getSharedFaces();
					Edge* nextB = nullptr;

					for (Face* f : sharedFacesB) 
					{
						Quad* quad = dynamic_cast<Quad*>(f);
						if (!quad) continue;

						if (std::find(visitedQuads.begin(), visitedQuads.end(), quad) == visitedQuads.end() && 
							visitedB.find(quad) == visitedB.end()) 
						{
							visitedB.insert(quad);

							bool idAlreadyPresent = false;

							for (Quad* qTest : visitedQuads) 
							{
								if (qTest && qTest->getID() == quad->getID()) 
								{
									idAlreadyPresent = true;
									break;
								}
							}

							if (!idAlreadyPresent) 
							{
								visitedQuads.push_back(quad);
							}
						
						for (Edge* e : quad->getEdgesArray()) 
						{
							if (e == currentB) continue;	

							Vertice* eStart = e->getStart();
							Vertice* eEnd = e->getEnd();
							Vertice* currStart = currentB->getStart();
							Vertice* currEnd = currentB->getEnd();

								if ((eStart != currStart && eStart != currEnd) && (eEnd != currStart && eEnd != currEnd)) 
								{
									nextB = e;
									break;
								}
							}

							break;
						}
					}
					
					if (nextB && std::find(DirectionA.begin(), DirectionA.end(), nextB) != DirectionA.end()) 
					{
						break;
					}
					
					if (nextB) 
					{
						currentB = nextB;
						continueB = true;
					}
				}
				

				if (!continueA && !continueB) break;
			}
		}

		std::vector<Edge*> entryEdges;

		if (!DirectionA.empty()) entryEdges.push_back(DirectionA.back());
		if (!DirectionB.empty()) entryEdges.push_back(DirectionB.back());

		for (Edge* entryEdge : entryEdges) 
		{
			const auto& entrySharedFaces = entryEdge->getSharedFaces();

			for (Face* f : entrySharedFaces) 
			{
				Quad* quad = dynamic_cast<Quad*>(f);
				if (!quad) continue;

				for (Quad* visited : visitedQuads) 
				{
					if (visited == quad && alreadyPrinted.find(quad) == alreadyPrinted.end()) 
					{
						std::cout << "entry Edge quad has been visited " << std::endl;
						alreadyPrinted.insert(quad);
					}
				}

				Edge* newExit = nullptr;
				for (Edge* e : quad->getEdgesArray()) 
				{
					if (e == entryEdge) continue;

					Vertice* eStart = e->getStart();
					Vertice* eEnd = e->getEnd();
					Vertice* entryStart = entryEdge->getStart();
					Vertice* entryEnd = entryEdge->getEnd();

					if ((eStart != entryStart && eStart != entryEnd) && (eEnd != entryStart && eEnd != entryEnd)) 
					{
						newExit = e;
						break;
					}
				}
			}
		}


		// ---- at the end of the loop we add the joining quad if it exists so that we can close the loop and cut the quads traversed ---- //
		if (!DirectionA.empty() && !DirectionB.empty()) 
		{
			Edge* lastEdgeA = DirectionA.back();  
			Edge* lastEdgeB = DirectionB.back();  
			
			const auto& sharedFacesA = lastEdgeA->getSharedFaces();
			const auto& sharedFacesB = lastEdgeB->getSharedFaces();
			
			for (Face* faceA : sharedFacesA) 
			{
				Quad* quadA = dynamic_cast<Quad*>(faceA);
				if (!quadA) continue;
				
				for (Face* faceB : sharedFacesB) 
				{
					Quad* quadB = dynamic_cast<Quad*>(faceB);
					if (!quadB) continue;
					
					if (quadA == quadB) 
					{
						quadA->isJoiningQuad = true;
						JoiningQuad.push_back(quadA);
						visitedQuads.push_back(quadA);
						break;
					}
				}
				if (!JoiningQuad.empty()) break;
			}
		}


		// ---- push in the loop ---- //


		// ---- Helper that checks if an edge is already in the loop ---- //
		auto edgeExistsInLoop = [&](Edge* edge) 
		{
			for (Edge* e : loop) 
			{
				if (e && edge && e->getID() == edge->getID()) return true;
			}
			return false;
		};

	

		// fill the loop with DirectionA and DirectionB edges
		for (Edge* edge : DirectionA)
		{
			if (edge && !edgeExistsInLoop(edge))
			{
				loop.push_back(edge);
			}
		}

		for (auto it = DirectionB.rbegin(); it != DirectionB.rend(); ++it)
		{
			Edge* edge = *it;
			if (edge && !edgeExistsInLoop(edge))
			{
				loop.push_back(edge);
			}
		}

		// ---- Draw the ghost loop ---- //

		CreatePerpendicularEdgeLoopGhost(DirectionA, DirectionB, scene, oglChildPos, oglChildSize);
	
		if (ImGui::IsKeyPressed(ImGuiKey_E) && JoiningQuad.size() == 1)
		{
			

			for (int i = 0; i < visitedQuads.size(); ++i)
			{
				if (!visitedQuads[i] || !visitedQuads[i]->isJoiningQuad) continue;
				
				for (int j = i + 1; j < visitedQuads.size(); )
				{
					if (visitedQuads[j] && visitedQuads[j]->isJoiningQuad && visitedQuads[i]->getID() == visitedQuads[j]->getID())
					{
						std::cout << "[EDGE LOOP] Removing duplicate joining quad with ID: " << visitedQuads[j]->getID() << std::endl;
						visitedQuads.erase(visitedQuads.begin() + j);
					}
					else
					{
						++j;
					}
				}
			}

			MeshEdit::CutQuad(loop, mesh, visitedQuads);

			visitedQuads.clear();
			JoiningQuad.clear();			
			loop.clear();

			selectedEdge = nullptr;

		}		

		return loop;
	}

	void CreatePerpendicularEdgeLoopGhost(const std::vector<Edge*>& directionA, const std::vector<Edge*>& directionB, ThreeDScene* scene, const ImVec2& oglChildPos, const ImVec2& oglChildSize)
	{
		if ((directionA.empty() && directionB.empty()) || !scene) return;
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		glm::mat4 view = scene->getViewMatrix();
		glm::mat4 proj = scene->getProjectionMatrix();

		if (directionA.size() > 1)
		{
			std::vector<glm::vec3> centersA;
			for (Edge* e : directionA)
			{
				if (!e) continue;
				Vertice* va = e->getStart();
				Vertice* vb = e->getEnd();
				ThreeDObject* parent = va->getMeshParent() ? va->getMeshParent() : (vb ? vb->getMeshParent() : nullptr);
				glm::mat4 model = parent ? parent->getModelMatrix() : glm::mat4(1.0f);
				glm::vec3 pa = glm::vec3(model * glm::vec4(va->getLocalPosition(), 1.0f));
				glm::vec3 pb = glm::vec3(model * glm::vec4(vb->getLocalPosition(), 1.0f));
				glm::vec3 center = 0.5f * (pa + pb);
				centersA.push_back(center);
			}

			for (size_t i = 0; i < centersA.size() - 1; ++i)
			{
				glm::vec3 cA = centersA[i];
				glm::vec3 cB = centersA[i + 1];
				glm::vec4 worldA(cA, 1.0f);
				glm::vec4 worldB(cB, 1.0f);
				glm::vec4 clipA = proj * view * worldA;
				glm::vec4 clipB = proj * view * worldB;
				if (clipA.w != 0.0f && clipB.w != 0.0f)
				{
					ImVec2 screenA = ImVec2(
						oglChildPos.x + oglChildSize.x * (0.5f + 0.5f * (clipA.x / clipA.w)),
						oglChildPos.y + oglChildSize.y * (0.5f - 0.5f * (clipA.y / clipA.w))
					);
					ImVec2 screenB = ImVec2(
						oglChildPos.x + oglChildSize.x * (0.5f + 0.5f * (clipB.x / clipB.w)),
						oglChildPos.y + oglChildSize.y * (0.5f - 0.5f * (clipB.y / clipB.w))
					);
					drawList->AddLine(screenA, screenB, IM_COL32(255,165,0,255), 4.0f); 
				}
			}
		}

		if (directionB.size() > 1)
		{
			std::vector<glm::vec3> centersB;
			for (Edge* e : directionB)
			{
				if (!e) continue;
				Vertice* va = e->getStart();
				Vertice* vb = e->getEnd();
				ThreeDObject* parent = va->getMeshParent() ? va->getMeshParent() : (vb ? vb->getMeshParent() : nullptr);
				glm::mat4 model = parent ? parent->getModelMatrix() : glm::mat4(1.0f);
				glm::vec3 pa = glm::vec3(model * glm::vec4(va->getLocalPosition(), 1.0f));
				glm::vec3 pb = glm::vec3(model * glm::vec4(vb->getLocalPosition(), 1.0f));
				glm::vec3 center = 0.5f * (pa + pb);
				centersB.push_back(center);
			}

			for (size_t i = 0; i < centersB.size() - 1; ++i)
			{
				glm::vec3 cA = centersB[i];
				glm::vec3 cB = centersB[i + 1];
				glm::vec4 worldA(cA, 1.0f);
				glm::vec4 worldB(cB, 1.0f);
				glm::vec4 clipA = proj * view * worldA;
				glm::vec4 clipB = proj * view * worldB;
				if (clipA.w != 0.0f && clipB.w != 0.0f)
				{
					ImVec2 screenA = ImVec2(
						oglChildPos.x + oglChildSize.x * (0.5f + 0.5f * (clipA.x / clipA.w)),
						oglChildPos.y + oglChildSize.y * (0.5f - 0.5f * (clipA.y / clipA.w))
					);
					ImVec2 screenB = ImVec2(
						oglChildPos.x + oglChildSize.x * (0.5f + 0.5f * (clipB.x / clipB.w)),
						oglChildPos.y + oglChildSize.y * (0.5f - 0.5f * (clipB.y / clipB.w))
					);
					drawList->AddLine(screenA, screenB, IM_COL32(255,165,0,255), 4.0f); 
				}
			}
		}

		if (!directionA.empty() && !directionB.empty()) 
		{
			Edge* lastEdgeA = directionA.back();
			Edge* lastEdgeB = directionB.back();
			
			if (lastEdgeA && lastEdgeB) 
			{
				Vertice* joinVert = nullptr;
				const auto& sharedFacesA = lastEdgeA->getSharedFaces();
				const auto& sharedFacesB = lastEdgeB->getSharedFaces();
				
				for (Face* faceA : sharedFacesA) 
				{
					Quad* quadA = dynamic_cast<Quad*>(faceA);
					if (!quadA) continue;
					
					for (Face* faceB : sharedFacesB) 
					{
						Quad* quadB = dynamic_cast<Quad*>(faceB);
						if (!quadB) continue;
						
						if (quadA == quadB) 
						{
							const auto& vertices = quadA->getVerticesArray();
							glm::vec3 center(0.0f);

							for (Vertice* v : vertices) 
							{
								if (v) center += v->getLocalPosition();
							}
							center /= 4.0f;
							
							joinVert = new Vertice();
							joinVert->setLocalPosition(center);
							break;
						}
					}
					if (joinVert) break;
				}
				
				if (joinVert) 
				{
					ThreeDObject* parent = nullptr;
					if (lastEdgeA->getStart()) 
					{
						parent = lastEdgeA->getStart()->getMeshParent();
					} 
					else if (lastEdgeB->getStart())
 					{
						parent = lastEdgeB->getStart()->getMeshParent();
					}
					
					glm::mat4 model = parent ? parent->getModelMatrix() : glm::mat4(1.0f);
					
					glm::vec3 centerA = 0.5f * (
						glm::vec3(model * glm::vec4(lastEdgeA->getStart()->getLocalPosition(), 1.0f)) +
						glm::vec3(model * glm::vec4(lastEdgeA->getEnd()->getLocalPosition(), 1.0f))
					);
					
					glm::vec3 centerB = 0.5f * (
						glm::vec3(model * glm::vec4(lastEdgeB->getStart()->getLocalPosition(), 1.0f)) +
						glm::vec3(model * glm::vec4(lastEdgeB->getEnd()->getLocalPosition(), 1.0f))
					);
					
					glm::vec3 joinPos = glm::vec3(model * glm::vec4(joinVert->getLocalPosition(), 1.0f));
					
					glm::vec4 clipA = proj * view * glm::vec4(centerA, 1.0f);
					glm::vec4 clipB = proj * view * glm::vec4(centerB, 1.0f);
					glm::vec4 clipJoin = proj * view * glm::vec4(joinPos, 1.0f);
					
					if (clipA.w != 0.0f && clipB.w != 0.0f && clipJoin.w != 0.0f) {
						ImVec2 screenA = ImVec2(
							oglChildPos.x + oglChildSize.x * (0.5f + 0.5f * (clipA.x / clipA.w)),
							oglChildPos.y + oglChildSize.y * (0.5f - 0.5f * (clipA.y / clipA.w))
						);
						ImVec2 screenB = ImVec2(
							oglChildPos.x + oglChildSize.x * (0.5f + 0.5f * (clipB.x / clipB.w)),
							oglChildPos.y + oglChildSize.y * (0.5f - 0.5f * (clipB.y / clipB.w))
						);
						ImVec2 screenJoin = ImVec2(
							oglChildPos.x + oglChildSize.x * (0.5f + 0.5f * (clipJoin.x / clipJoin.w)),
							oglChildPos.y + oglChildSize.y * (0.5f - 0.5f * (clipJoin.y / clipJoin.w))
						);
						
						drawList->AddLine(screenA, screenJoin, IM_COL32(255,165,0,255), 3.0f);
						drawList->AddLine(screenJoin, screenB, IM_COL32(255,165,0,255), 3.0f);
					}
					
					delete joinVert;
				}
			}
		}

		createGhostVertices(directionA, directionB, scene, oglChildPos, oglChildSize);
	}

	void createGhostVertices(const std::vector<Edge*>& directionA, const std::vector<Edge*>& directionB, ThreeDScene* scene, const ImVec2& oglChildPos, const ImVec2& oglChildSize)
	{
		if ((directionA.empty() && directionB.empty()) || !scene) return;
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		glm::mat4 view = scene->getViewMatrix();
		glm::mat4 proj = scene->getProjectionMatrix();

		for (Edge* e : directionA)
		{
			if (!e) continue;
			Vertice* va = e->getStart();
			Vertice* vb = e->getEnd();
			ThreeDObject* parent = va->getMeshParent() ? va->getMeshParent() : (vb ? vb->getMeshParent() : nullptr);
			glm::mat4 model = parent ? parent->getModelMatrix() : glm::mat4(1.0f);
			glm::vec3 pa = glm::vec3(model * glm::vec4(va->getLocalPosition(), 1.0f));
			glm::vec3 pb = glm::vec3(model * glm::vec4(vb->getLocalPosition(), 1.0f));
			glm::vec3 ghostPos = 0.5f * (pa + pb);
			glm::vec4 worldGhost(ghostPos, 1.0f);
			glm::vec4 clipGhost = proj * view * worldGhost;
			if (clipGhost.w != 0.0f)
			{
				ImVec2 screenGhost = ImVec2(
					oglChildPos.x + oglChildSize.x * (0.5f + 0.5f * (clipGhost.x / clipGhost.w)),
					oglChildPos.y + oglChildSize.y * (0.5f - 0.5f * (clipGhost.y / clipGhost.w))
				);
				drawList->AddCircleFilled(screenGhost, 7.0f, IM_COL32(255,0,0,255)); 
			}
		}

		for (Edge* e : directionB)
		{
			if (!e) continue;
			Vertice* va = e->getStart();
			Vertice* vb = e->getEnd();
			ThreeDObject* parent = va->getMeshParent() ? va->getMeshParent() : (vb ? vb->getMeshParent() : nullptr);
			glm::mat4 model = parent ? parent->getModelMatrix() : glm::mat4(1.0f);
			glm::vec3 pa = glm::vec3(model * glm::vec4(va->getLocalPosition(), 1.0f));
			glm::vec3 pb = glm::vec3(model * glm::vec4(vb->getLocalPosition(), 1.0f));
			glm::vec3 ghostPos = 0.5f * (pa + pb);
			glm::vec4 worldGhost(ghostPos, 1.0f);
			glm::vec4 clipGhost = proj * view * worldGhost;
			if (clipGhost.w != 0.0f)
			{
				ImVec2 screenGhost = ImVec2(
					oglChildPos.x + oglChildSize.x * (0.5f + 0.5f * (clipGhost.x / clipGhost.w)),
					oglChildPos.y + oglChildSize.y * (0.5f - 0.5f * (clipGhost.y / clipGhost.w))
				);
				drawList->AddCircleFilled(screenGhost, 7.0f, IM_COL32(255,0,0,255)); // Rouge pour tous les ghost vertices 
			}
	}
	}
}