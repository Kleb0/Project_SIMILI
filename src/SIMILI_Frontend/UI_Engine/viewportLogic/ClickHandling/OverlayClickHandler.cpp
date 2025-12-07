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
#include "KeyManagement/KeyManager.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <iostream>

OverlayClickHandler::OverlayClickHandler(OverlayViewport* owner) : viewport(owner) {}

static bool isShiftPressed() 
{
	auto& keyManager = SIMILI::Input::KeyManager::getInstance();
	auto* inputSystem = keyManager.getInputSystem();
	if (!inputSystem) {
		std::cout << "[isShiftPressed] ERROR: inputSystem is nullptr" << std::endl;
		return false;
	}
	
	inputSystem->pollKeyStates();
	
	// Check generic Shift (16) OR specific left/right shifts
	bool genericShift = inputSystem->isKeyPressed(VK_SHIFT);
	bool leftShift = inputSystem->isKeyPressed(VK_LSHIFT);
	bool rightShift = inputSystem->isKeyPressed(VK_RSHIFT);
	
	std::cout << "[isShiftPressed] Generic: " << genericShift
			  << " | LeftShift: " << leftShift 
			  << " | RightShift: " << rightShift 
			  << " | VK_SHIFT=" << VK_SHIFT
			  << " | VK_LSHIFT=" << VK_LSHIFT 
			  << " | VK_RSHIFT=" << VK_RSHIFT << std::endl;
	
	return genericShift || leftShift || rightShift;
}

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

	if (mouseX < 0 || mouseX >= viewport->getWidth() ||
		mouseY < 0 || mouseY >= viewport->getHeight())
	{
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
		ThreeDObjectSelector* selector = viewport->getSelector();

		if (!selector) 
		{
			std::cerr << "[OVERLAY CLICK HANDLER] Error: No selector available" << std::endl;
			return;
		}

		if (currentMode == normalMode)
		{
			bool shiftPressed = isShiftPressed();
			
			if (shiftPressed)
			{
				std::cout << "TEST TEST TEST TEST TEST !!!! Raycast called with left shift pressed !" << std::endl;
			}
			
			// Don't perform raycast if hovering over gizmo (even if not actively using it)
			bool preventSelection = ImGuizmo::IsOver();
			
			auto currentSelection = viewport->getMultipleSelectedObjects();
			std::list<ThreeDObject*> multipleSelected(currentSelection.begin(), currentSelection.end());
			
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

			if (selected)
			{
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
				}
				else if (shiftPressed)
				{
					multipleSelected.erase(it);
					selected->setSelected(false);
				}

				viewport->setMultipleSelectedObjects(multipleSelected);
				selector->clearTarget();

			}
			else if (!preventSelection)
			{
				for (auto* obj : objects) obj->setSelected(false);
				std::list<ThreeDObject*> empty;
				viewport->setMultipleSelectedObjects(empty);
				selector->clearTarget();
			}
		}

		if (currentMode == verticeMode)
		{
			bool shiftPressed = isShiftPressed();
			
			if (shiftPressed)
			{
				std::cout << "TEST TEST TEST TEST TEST !!!! Vertice raycast called with shift pressed !" << std::endl;
			}
			
			bool preventSelection = ImGuizmo::IsOver() || ImGuizmo::IsUsing();

			std::cout << "[VerticeMode] Click - Shift: " << (shiftPressed ? "YES" : "NO") 
					<< " | Gizmo prevent: " << (preventSelection ? "YES" : "NO") << std::endl;

			auto& multipleVertices = viewport->getMultipleSelectedVertices();

			Vertice* selectedVertice = nullptr;
			if (!preventSelection)
			{
				selectedVertice = selector->pickUpVertice(
					mouseX, mouseY,
					windowWidth, windowHeight, view, proj,
					objects, false  
				);
				std::cout << "[VerticeMode] pickUpVertice returned: " 
						<< (selectedVertice ? "VERTICE FOUND" : "nullptr") << std::endl;
			}
			else
			{
				std::cout << "[OVERLAY CLICK HANDLER] Vertice raycast skipped - mouse over or using ImGuizmo" << std::endl;
			}

			if (selectedVertice)
			{

				if (!shiftPressed)
				{
					for (ThreeDObject* obj : objects)
					{
						Mesh* mesh = dynamic_cast<Mesh*>(obj);
						if (!mesh) continue;    
						for (Vertice* v : mesh->getVertices()) v->setSelected(false);
					}
					multipleVertices.clear();
				}

				auto it = std::find(multipleVertices.begin(), multipleVertices.end(), selectedVertice);
				
				if (it == multipleVertices.end())
				{
					multipleVertices.push_back(selectedVertice);
					selectedVertice->setSelected(true);
					std::cout << "[VerticeMode] Vertice added to selection" << std::endl;
				}
				else if (shiftPressed)
				{
					multipleVertices.erase(it);
					selectedVertice->setSelected(false);
					std::cout << "[VerticeMode] SHIFT REMOVE - Vertice removed from selection" << std::endl;
				}
				
				std::cout << "[VerticeMode] Total selected vertices: " << multipleVertices.size() << std::endl;
			}
			else if (!preventSelection)
			{
				if (!shiftPressed)
				{
					for (ThreeDObject* obj : objects)
					{
						Mesh* mesh = dynamic_cast<Mesh*>(obj);
						if (!mesh) continue;    
						for (Vertice* vert : mesh->getVertices()) vert->setSelected(false);
					}
					multipleVertices.clear();
				}
			}
		}		

		if (currentMode == faceMode)
		{
			bool shiftPressed = isShiftPressed();
			
			if (shiftPressed)
			{
				std::cout << "TEST TEST TEST TEST TEST !!!! Face raycast called with shift pressed !" << std::endl;
			}
			
			bool preventSelection = ImGuizmo::IsOver() || ImGuizmo::IsUsing();

			auto& multipleFaces = viewport->getMultipleSelectedFaces();

			Face* selectedFace = nullptr;
			if (!preventSelection)
			{
				selectedFace = selector->pickupFace(
					mouseX, mouseY,
					windowWidth, windowHeight, view, proj, objects, false  // Always preserve selection for manual management
				);
			}

			if (selectedFace)
			{
				// If Shift is NOT pressed, clear previous selection
				if (!shiftPressed)
				{
					for (ThreeDObject* obj : objects)
					{
						Mesh* mesh = dynamic_cast<Mesh*>(obj);
						if (!mesh) continue;
						for (Face* f : mesh->getFaces()) if (f) f->setSelected(false);
					}
					multipleFaces.clear();
				}

				auto it = std::find(multipleFaces.begin(), multipleFaces.end(), selectedFace);
				
				if (it == multipleFaces.end())
				{
					multipleFaces.push_back(selectedFace);
					selectedFace->setSelected(true);
					std::cout << "[FaceMode] Face added to selection" << std::endl;
				}
				else if (shiftPressed)
				{
					multipleFaces.erase(it);
					selectedFace->setSelected(false);
					std::cout << "[FaceMode] SHIFT REMOVE - Face removed from selection" << std::endl;
				}
				
				std::cout << "[FaceMode] Total selected faces: " << multipleFaces.size() << std::endl;
			}
			else if (!preventSelection)
			{
				if (!shiftPressed)
				{
					for (ThreeDObject* obj : objects)
					{
						Mesh* mesh = dynamic_cast<Mesh*>(obj);
						if (!mesh) continue;
						for (Face* f : mesh->getFaces()) if (f) f->setSelected(false);
					}
					multipleFaces.clear();
				}
			}
		}	
		
		if (currentMode == edgeMode)
		{
			bool shiftPressed = isShiftPressed();
			
			if (shiftPressed)
			{
				std::cout << "TEST TEST TEST TEST TEST !!!! Edge raycast called with shift pressed !" << std::endl;
			}
			
			bool preventSelection = ImGuizmo::IsOver() || ImGuizmo::IsUsing();

			auto& multipleEdges = viewport->getMultipleSelectedEdges();

			Edge* selectedEdge = nullptr;
			if (!preventSelection)
			{
				selectedEdge = selector->pickupEdge(
					mouseX, mouseY,
					windowWidth, windowHeight, view, proj, objects, false  // Always preserve selection for manual management
				);
			}

			if (selectedEdge)
			{
				// If Shift is NOT pressed, clear previous selection
				if (!shiftPressed)
				{
					for (ThreeDObject* obj : objects)
					{
						Mesh* mesh = dynamic_cast<Mesh*>(obj);
						if (!mesh) continue;    
						for (Edge* e : mesh->getEdges()) if (e) e->setSelected(false);
					}
					multipleEdges.clear();
				}

				auto it = std::find(multipleEdges.begin(), multipleEdges.end(), selectedEdge);
				
				if (it == multipleEdges.end())
				{
					multipleEdges.push_back(selectedEdge);
					selectedEdge->setSelected(true);
					std::cout << "[EdgeMode] Edge added to selection" << std::endl;
				}
				else if (shiftPressed)
				{
					multipleEdges.erase(it);
					selectedEdge->setSelected(false);
					std::cout << "[EdgeMode] SHIFT REMOVE - Edge removed from selection" << std::endl;
				}
				
				std::cout << "[EdgeMode] Total selected edges: " << multipleEdges.size() << std::endl;
			}
			else if (!preventSelection)
			{
				if (!shiftPressed)
				{
					for (ThreeDObject* obj : objects)
					{
						Mesh* mesh = dynamic_cast<Mesh*>(obj);
						if (!mesh) continue;    
						for (Edge* e : mesh->getEdges()) if (e) e->setSelected(false);
					}
					multipleEdges.clear();
				}
			}
		}
}