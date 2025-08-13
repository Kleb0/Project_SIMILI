#include "UI/UIDocking/Uidocking.hpp"
#include "imgui_internal.h"

void Uidocking::SetupDefaultDockspace(ImGuiID dockspace_id)
{
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    ImGuiID dock_main_id = dockspace_id;
    ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);

    ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.12f, nullptr, &dock_main_id);
    ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.22f, nullptr, &dock_main_id);
    ImGuiID dock_id_center = dock_main_id;

    ImGui::DockBuilderDockWindow("Hierarchy Inspector", dock_id_left);
    ImGui::DockBuilderDockWindow("3D Viewport", dock_id_center);
    ImGui::DockBuilderDockWindow("Object Inspector", dock_id_right);
    ImGui::DockBuilderDockWindow("Project Viewer", dock_id_bottom);

    ImGui::DockBuilderFinish(dockspace_id);
}