#include "OverlayClickHandler.hpp"
#include "overlay_viewport.hpp"
#include "../../../Engine/ThreeDScene.hpp"
#include "../../../WorldObjects/Mesh/Mesh.hpp"
#include "../../../WorldObjects/Basic/Vertice.hpp"
#include "../../../WorldObjects/Basic/Face.hpp"
#include "../../../WorldObjects/Basic/Edge.hpp"
#include "../../../WorldObjects/Camera/Camera.hpp"
#include "../../../Engine/ThreeDObjectSelector.hpp"
#include "../../../UI/ThreeDModes/Normal_Mode.hpp"
#include "../../../UI/ThreeDModes/Vertice_Mode.hpp"
#include "../../../UI/ThreeDModes/Face_Mode.hpp"
#include "../../../UI/ThreeDModes/Edge_Mode.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <iostream>

OverlayClickHandler::OverlayClickHandler(OverlayViewport* owner) : viewport(owner) {}

void OverlayClickHandler::handle() 
{

	scene = viewport->getThreeDScene();
	if (!scene) 
	{
		std::cerr << "[OVERLAY CLICK HANDLER] Error: No ThreeDScene available" << std::endl;
		return;
	}

	POINT cursor_pos;
	if (!GetCursorPos(&cursor_pos)) 
	{
		std::cerr << "[OVERLAY CLICK HANDLER] Error: Failed to get cursor position" << std::endl;
		return;
	}
	
	HWND hwnd = viewport->getHandle();
	if (!hwnd) 
	{
		std::cerr << "[OVERLAY CLICK HANDLER] Error: Invalid window handle" << std::endl;
		return;
	}
	
	if (!ScreenToClient(hwnd, &cursor_pos)) {
		std::cerr << "[OVERLAY CLICK HANDLER] Error: Failed to convert screen to client coords" << std::endl;
		return;
	}
	
	int mouseX = cursor_pos.x;
	int mouseY = cursor_pos.y;

	// std::cout << "[OVERLAY CLICK HANDLER] Mouse click at: (" << mouseX << ", " << mouseY 
	//           << ") - Viewport size: " << viewport->getWidth() << "x" << viewport->getHeight() 
	//           << " | ImGuizmo Over: " << ImGuizmo::IsOver() << std::endl;

	if (mouseX < 0 || mouseX >= viewport->getWidth() ||
		mouseY < 0 || mouseY >= viewport->getHeight())
	{
		// std::cout << "[OVERLAY CLICK HANDLER] Click outside viewport bounds - ignoring" << std::endl;
		return;
	}

	int windowWidth = viewport->getWidth();
	int windowHeight = viewport->getHeight();

	glm::mat4 view = scene->getViewMatrix();
	glm::mat4 proj = scene->getProjectionMatrix();
	
	auto& listRef = scene->getObjectsRef();
	std::vector<ThreeDObject*> objects;
		
		if (listRef.empty()) 
		{
			// std::cout << "[OVERLAY CLICK HANDLER] No objects in scene or scene not ready" << std::endl;
			return;
		}
		
		objects.reserve(listRef.size());
		for (auto* o : listRef) 
		{
			if (o) objects.push_back(o);
		}

		ThreeDMode* currentMode = viewport->getCurrentMode();
		Normal_Mode* normalMode = viewport->getNormalMode();
		Vertice_Mode* verticeMode = viewport->getVerticeMode();
		Face_Mode* faceMode = viewport->getFaceMode();
		Edge_Mode* edgeMode = viewport->getEdgeMode();

		// Get selector from viewport
		ThreeDObjectSelector* selector = viewport->getSelector();
		if (!selector) {
			std::cerr << "[OVERLAY CLICK HANDLER] Error: No selector available" << std::endl;
			return;
		}

		if (currentMode == normalMode)
		{
			// Don't perform raycast if hovering over gizmo (even if not actively using it)
			bool preventSelection = ImGuizmo::IsOver();
			
			if (!preventSelection)
			{
				selector->pickUpMesh(mouseX, mouseY,
					windowWidth, windowHeight, view, proj, objects);
			}
			else 
			{
				std::cout << "[OVERLAY CLICK HANDLER] Raycast skipped - mouse over ImGuizmo" << std::endl;
			}

			ThreeDObject* selected = selector->getSelectedObject();
			bool shiftPressed = ImGui::GetIO().KeyShift;

			if (selected)
			{
				auto currentSelection = viewport->getMultipleSelectedObjects();
				std::list<ThreeDObject*> multipleSelected(currentSelection.begin(), currentSelection.end());

				if (!shiftPressed)
				{
					for (auto* obj : objects) obj->setSelected(false);
					multipleSelected.clear();
				}

				auto it = std::find(multipleSelected.begin(), multipleSelected.end(), selected);
				
				if (it == multipleSelected.end())
				{
					multipleSelected.push_back(selected);
					selected->setSelected(true);
					
					try 
					{
						glm::mat4 globalMatrix = selected->getGlobalModelMatrix();
						glm::vec3 worldPos = glm::vec3(globalMatrix[3]);
						// std::cout << "[OVERLAY CLICK HANDLER] : Selected object world position: ("
						//         << worldPos.x << ", "
						//         << worldPos.y << ", "
						//         << worldPos.z << ")" 
						// << " for object with name " << selected->getName() << std::endl;
					} 
					catch (const std::exception& e) 
					{
						std::cerr << "[OVERLAY CLICK HANDLER] Error getting object position: " << e.what() << std::endl;
					}
				}
				else if (shiftPressed)
				{
					multipleSelected.erase(it);
					selected->setSelected(false);
				}

				viewport->setMultipleSelectedObjects(multipleSelected);
				// std::cout << "[OVERLAY CLICK HANDLER] Multiple selected objects count: " 
				//           << multipleSelected.size() << std::endl;
				selector->clearTarget();
			}
			else if (!preventSelection)  // Only deselect if we weren't over gizmo
			{
				for (auto* obj : objects) obj->setSelected(false);
				std::list<ThreeDObject*> empty;
				viewport->setMultipleSelectedObjects(empty);
				selector->clearTarget();
				// std::cout << "[OVERLAY CLICK HANDLER] Deselected all objects" << std::endl;
			}
		}

		if (currentMode == verticeMode)
		{
			// Don't perform raycast if hovering over gizmo (same logic as Normal mode)
			bool preventSelection = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
			bool shiftPressed = ImGui::GetIO().KeyShift;

			Vertice* selectedVertice = nullptr;
			if (!preventSelection)
			{
				selectedVertice = selector->pickUpVertice(
					mouseX, mouseY,
					windowWidth, windowHeight, view, proj,
					objects, shiftPressed
				);
			}
			else
			{
				// std::cout << "[OVERLAY CLICK HANDLER] Vertice raycast skipped - mouse over or using ImGuizmo" << std::endl;
			}

			if (selectedVertice)
			{
				// std::cout << "[OVERLAY CLICK HANDLER] Vertice selected in Vertice Mode" << std::endl;
				
				auto& multipleVertices = viewport->getMultipleSelectedVertices();
				
				if (shiftPressed)
				{
					auto it = std::find(multipleVertices.begin(), multipleVertices.end(), selectedVertice);
					if (it == multipleVertices.end())
					{
						multipleVertices.push_back(selectedVertice);
						for (Vertice* v : multipleVertices) v->setSelected(true);
					}
				}
				else
				{
					for (ThreeDObject* obj : objects)
					{
						Mesh* mesh = dynamic_cast<Mesh*>(obj);
						if (!mesh) continue;    
						for (Vertice* v : mesh->getVertices()) v->setSelected(false);
					}
					multipleVertices.clear();
					selectedVertice->setSelected(true);
					multipleVertices.push_back(selectedVertice);
				}
			}
			else
			{
				if (!shiftPressed && !preventSelection)  // Only deselect if we weren't over gizmo
				{
					for (ThreeDObject* obj : objects)
					{
						Mesh* mesh = dynamic_cast<Mesh*>(obj);
						if (!mesh) continue;    
						for (Vertice* vert : mesh->getVertices()) vert->setSelected(false);
					}
					viewport->getMultipleSelectedVertices().clear();
				}
			}
		}

		if (currentMode == faceMode)
		{
			// Don't perform raycast if hovering over gizmo (same logic as Normal mode)
			bool preventSelection = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
			bool shiftPressed = ImGui::GetIO().KeyShift;

			Face* selectedFace = nullptr;
			if (!preventSelection)
			{
				selectedFace = selector->pickupFace(
					mouseX, mouseY,
					windowWidth, windowHeight, view, proj, objects, shiftPressed
				);
			}
			else
			{
				// std::cout << "[OVERLAY CLICK HANDLER] Face raycast skipped - mouse over or using ImGuizmo" << std::endl;
			}

			if (selectedFace)
			{
				// std::cout << "[OVERLAY CLICK HANDLER] Face selected in Face Mode" << std::endl;
				
				auto& multipleFaces = viewport->getMultipleSelectedFaces();
				
				if (shiftPressed)
				{
					auto it = std::find(multipleFaces.begin(), multipleFaces.end(), selectedFace);
					if (it == multipleFaces.end())
					{
						multipleFaces.push_back(selectedFace);
						for (Face* f : multipleFaces) if (f) f->setSelected(true);
					}
				}
				else
				{
					for (ThreeDObject* obj : objects)
					{
						Mesh* mesh = dynamic_cast<Mesh*>(obj);
						if (!mesh) continue;
						for (Face* f : mesh->getFaces()) if (f) f->setSelected(false);
					}
					multipleFaces.clear();
					selectedFace->setSelected(true);
					multipleFaces.push_back(selectedFace);
				}
			}
			else
			{
				if (!shiftPressed && !preventSelection)  // Only deselect if we weren't over gizmo
				{
					for (ThreeDObject* obj : objects)
					{
						Mesh* mesh = dynamic_cast<Mesh*>(obj);
						if (!mesh) continue;
						for (Face* f : mesh->getFaces()) if (f) f->setSelected(false);
					}
					viewport->getMultipleSelectedFaces().clear();
				}
			}
		}

		if (currentMode == edgeMode)
		{

			bool preventSelection = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
			bool shiftPressed = ImGui::GetIO().KeyShift;

			Edge* selectedEdge = nullptr;
			if (!preventSelection)
			{
				selectedEdge = selector->pickupEdge(
					mouseX, mouseY,
					windowWidth, windowHeight, view, proj, objects, shiftPressed
				);
			}
			else
			{
				// std::cout << "[OVERLAY CLICK HANDLER] Edge raycast skipped - mouse over or using ImGuizmo" << std::endl;
			}

			if (selectedEdge)
			{
				// std::cout << "[OVERLAY CLICK HANDLER] Edge selected in Edge Mode" << std::endl;
				
				auto& multipleEdges = viewport->getMultipleSelectedEdges();
				
				if (shiftPressed)
				{
					auto it = std::find(multipleEdges.begin(), multipleEdges.end(), selectedEdge);
					if (it == multipleEdges.end())
					{
						multipleEdges.push_back(selectedEdge);
						for (Edge* e : multipleEdges) if (e) e->setSelected(true);
					}
				}
				else
				{
					for (ThreeDObject* obj : objects)
					{
						Mesh* mesh = dynamic_cast<Mesh*>(obj);
						if (!mesh) continue;    
						for (Edge* e : mesh->getEdges()) if (e) e->setSelected(false);
					}
					multipleEdges.clear();
					selectedEdge->setSelected(true);
					multipleEdges.push_back(selectedEdge);
				}
			}
			else
			{
				if (!shiftPressed && !preventSelection) 
				{
					for (ThreeDObject* obj : objects)
					{
						Mesh* mesh = dynamic_cast<Mesh*>(obj);
						if (!mesh) continue;    
						for (Edge* e : mesh->getEdges()) if (e) e->setSelected(false);
					}
					viewport->getMultipleSelectedEdges().clear();
				}
			}
		}
}
