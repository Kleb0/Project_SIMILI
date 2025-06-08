#include <windows.h>

#include <imgui_internal.h>

#include "UI/ObjectInspector.hpp"
#include "UI/ContextualMenu.hpp"
#include "Engine/OpenGLContext.hpp"
#include "WorldObjects/Cube.hpp"
#include <iostream>

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
    std::cout << "[ContextualMenu] Contextual menu opened at position: " << popupPos.x << ", " << popupPos.y << std::endl;
}

void ContextualMenu::hide()
{
    if (!isOpen)
        return;

    isOpen = false;
    std::cout << "[ContextualMenu] Contextual menu closed." << std::endl;
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
                    if (threeDWindow && hierarchyInspector)
                    {

                        if (objectInspector)
                            objectInspector->clearInspectedObject();

                        ThreeDObject *toDelete = selected;

                        std::string msg = "About to delete object: " + toDelete->getName();
                        MessageBoxA(nullptr, msg.c_str(), "SIMILI DEBUG - DELETE", MB_OK | MB_ICONWARNING);

                        hierarchyInspector->unselectObject(toDelete);
                        pendingDeletion = toDelete;
                    }
                    hide();
                    return;
                }
            }
        }

        if (ImGui::MenuItem("Create Cube") && threeDWindow)
        {
            Cube *newCube = new Cube();
            newCube->setName("newCube");
            newCube->setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            threeDWindow->addThreeDObjectsToScene({newCube});

            std::cout << "[ContextualMenu] Created a new Cube object with name: " << newCube->getName() << std::endl;

            if (hierarchyInspector)
                hierarchyInspector->selectObject(newCube);

            hide();
        }

        ImGui::End();

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
        {
            hide();
        }

        if (pendingDeletion)
        {
            threeDWindow->removeThreeDObjectsFromScene({pendingDeletion});
            pendingDeletion = nullptr;
        }
    }
}
