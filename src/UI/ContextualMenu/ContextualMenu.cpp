#include <windows.h>

#include <imgui_internal.h>

#include "UI/ObjectInspectorLogic/ObjectInspector.hpp"
#include "UI/ContextualMenu/ContextualMenu.hpp"
#include "Engine/OpenGLContext.hpp"
#include "WorldObjects/Mesh/Mesh.hpp"
#include "Engine/PrimitivesCreation/CreatePrimitive.hpp"
#include "Engine/ErrorBox.hpp"
#include <iostream>

void ContextualMenu::setScene(ThreeDScene* s)
{
	scene = s;
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
			ThreeDObject *selected = hierarchyInspector->getSelectedObject();
			if (selected)
			{
				ImGui::Separator();
				ImGui::Text("Current Selected object : %s", selected->getName().c_str());

				if (ImGui::MenuItem("Delete selected object"))
				{
					if (objectInspector)
						objectInspector->clearInspectedObject();

                    ThreeDObject *toDelete = selected;
                    ThreeDObject *parent = toDelete->getParent();

                    if (parent)
                    {
                        parent->removeChild(toDelete);
                    }

                    scene->removeObject(toDelete);
           
                
					hide();
					ImGui::End();
					return;
				}
			}
		}

		if (ImGui::MenuItem("Create Cube") && scene)
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

		ImGui::End();

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
		{
			hide();
		}
	}
}
