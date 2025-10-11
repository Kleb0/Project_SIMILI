#include <windows.h>

#include <imgui_internal.h>

#include "UI/ObjectInspectorLogic/ObjectInspector.hpp"
#include "UI/ContextualMenu/ContextualMenu.hpp"
#include "Engine/OpenGLContext.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include "Engine/PrimitivesCreation/CreatePrimitive.hpp"
#include "Engine/ErrorBox.hpp"
#include <iostream>
#include <algorithm>

void ContextualMenu::setScene(ThreeDScene* s)
{
	scene = s;
}

void ContextualMenu::setMode(ThreeDMode *mode)
{
	currentMode = mode;
	//if the mode is from class faceMode
	

}

void ContextualMenu::setSelectedObject(ThreeDObject* obj)
{
	selectedObject = obj;
}

void ContextualMenu::setHierarchyInspector(HierarchyInspector *inspector)
{
	hierarchyInspector = inspector;
}

void ContextualMenu::setObjectInspector(ObjectInspector *inspector)
{
	objectInspector = inspector;
}

void ContextualMenu::show()
{

	popupPos = ImGui::GetMousePos();
	isOpen = true;
}

void ContextualMenu::hide()
{
	if (!isOpen)
		return;

	isOpen = false;
}

void ContextualMenu::setThreeDWindow(ThreeDWindow *window)
{
	threeDWindow = window;
}

void ContextualMenu::render()
{
	if (isOpen)
	{
		ImGui::SetNextWindowPos(popupPos, ImGuiCond_Always);
		ImGui::Begin("##ContextualMenu", nullptr,
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings);

		ImGui::Text("Contextual Menu");

		if (hierarchyInspector)
		{
			

			if (dynamic_cast<Face_Mode*>(currentMode))
			{
				ImGui::Text("Face Mode Active");
				ImGui::Separator();
				if (threeDWindow && threeDWindow->hasSelectedFace())
				{
					ImGui::Text("ThreeDWindow has a selected face");

					if (ImGui::MenuItem("Delete selected face"))
					{
						const std::list<Face*>& selectedFaces = threeDWindow->getmultipleSelectedFace();
						if (!selectedFaces.empty() && scene)
						{
							for (Face* selectedFace : selectedFaces)
							{
								Mesh* parentMesh = selectedFace->getParentMesh();
								if (parentMesh)
								{
									const auto& faceEdges = selectedFace->getEdges();
									
									for (Edge* edge : faceEdges)
									{
										if (edge)
										{
											std::vector<Face*> currentSharedFaces = edge->getSharedFaces();
											
											auto it = std::find(currentSharedFaces.begin(), currentSharedFaces.end(), selectedFace);
											if (it != currentSharedFaces.end())
											{
												currentSharedFaces.erase(it);
												edge->setSharedFaces(currentSharedFaces);
											}
										}
									}
									
									parentMesh->destroyFaces({ selectedFace });									
									parentMesh->destroyOrphanEdges();
									parentMesh->destroyOrphanVertices();
								}
							}
							
							if (threeDWindow)
							{
							    threeDWindow->clearSelectedFaces();
							}
						}
						hide();
						ImGui::End();
						return;
					}
				}
			}
		}

		if(dynamic_cast<Normal_Mode*>(currentMode))
		{

			ThreeDObject *selected = hierarchyInspector->getSelectedObject();

			ImGui::Text("Normal Mode Active");
			ImGui::Separator();

			if (selected)
			{
				ImGui::Separator();
				ImGui::Text("Current Selected object : %s", selected->getName().c_str());
				ImGui::Separator();

				if (ImGui::MenuItem("Delete selected object"))
				{
					if (objectInspector)
						objectInspector->clearInspectedObject();

                    ThreeDObject *toDelete = selected;      

                    scene->removeObject(toDelete);          
                
					hide();
					ImGui::End();
					return;
				}
			}

			if (ImGui::MenuItem("Create Cube"))
			{
				Mesh* newCube = Primitives::CreateCubeMesh(1.0f, glm::vec3(0.0f, 0.0f, 0.0f),"NewCube", true);
				scene->addObject(newCube);

				if (hierarchyInspector)
				{
					hierarchyInspector->selectObject(newCube);
					hierarchyInspector->redrawSlotsList();                            
				}               
			
				hide();
			}
		}
		
		ImGui::End();

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
		{
			hide();
		}
	}
}
