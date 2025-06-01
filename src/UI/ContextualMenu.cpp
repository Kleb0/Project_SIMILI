#include "UI/ContextualMenu.hpp"
#include <iostream>

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

        ImGui::Text("Hello Contextual Menu");

        ImGui::End();
    }
}
