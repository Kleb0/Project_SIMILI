#include "OptionsMenu.hpp"
#include <imgui.h>
#include <tinyfiledialogs.h>
#include <fstream>
#include <cstring>
#include <iostream>

OptionsMenuContent::OptionsMenuContent() {}

void OptionsMenuContent::render(const std::function<void(const std::string&)>& saveActiveSceneFunc, void* sceneRef)
{
    if (!optionsMenuHasbeenClicked)
        return;


    ImGui::SetNextWindowPos(optionsButtonPos, ImGuiCond_Always);
    ImGui::Begin("##OptionsMenu", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings);

    ImGui::Text("Options");
    ImGui::Separator();

    if (ImGui::MenuItem("Save current UI as..."))
    {
        const char *filterPatterns[1] = {"*.ini"};
        const char *savePath = tinyfd_saveFileDialog(
            "Save UI Layout",
            "custom_layout.ini",
            1,
            filterPatterns,
            "INI Layout Files");
        if (savePath)
        {
            const char *ini_data = ImGui::SaveIniSettingsToMemory();
            std::ofstream out(savePath, std::ios::binary | std::ios::trunc);
            if (out)
            {
                out.write(ini_data, std::strlen(ini_data));
                std::cout << "[UI_CREATOR_INFO] Layout saved to: " << savePath << std::endl;
            }
            else
            {
                std::cerr << "[ERROR] Failed to save layout to: " << savePath << std::endl;
            }
        }
    }
    if (ImGui::MenuItem("Load UI layout..."))
    {
        const char *filterPatterns[1] = {"*.ini"};
        const char *loadPath = tinyfd_openFileDialog(
            "Load UI Layout",
            "",
            1,
            filterPatterns,
            "INI Layout Files",
            0);
        if (loadPath)
        {
            // UiCreator::loadLayoutFromFile(loadPath);
            // UiCreator::saveLastLayoutPath(loadPath);
        }
    }
    if (ImGui::MenuItem("Save Scene as JSON"))
    {
        const char *filterPatterns[1] = {"*.json"};
        const char *savePath = tinyfd_saveFileDialog(
            "Save Scene",
            "scene.json",
            1,
            filterPatterns,
            "JSON Scene Files");
        if (savePath && saveActiveSceneFunc)
        {
            try
            {
                if (sceneRef)
                {
                    saveActiveSceneFunc(savePath);
                    std::cout << "[INFO] Scene saved to: " << savePath << std::endl;
                }
                else
                {
                    std::cerr << "[ERROR] No active scene to save." << std::endl;
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "[ERROR] Failed to save scene: " << e.what() << std::endl;
            }
        }
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Fermer"))
    {
        optionsMenuHasbeenClicked = false;
    }
    ImGui::End();

    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
    {
        optionsMenuHasbeenClicked = false;
    }
}
