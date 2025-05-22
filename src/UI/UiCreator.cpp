#include "UiCreator.hpp"
#include "imgui.h"

#include <fstream>
#include <string>
#include <sstream>
#include <iostream>
#include <cstring>
#include <filesystem>
namespace fs = std::filesystem;

namespace UiCreator
{
    std::string getLayoutFilePath(const std::string &filename)
    {

        return (fs::current_path().parent_path() / "src" / "resources" / filename).string();
    }

    void saveCurrentLayoutToDefault()
    {
        const char *ini_data = ImGui::SaveIniSettingsToMemory();
        std::ofstream out("src/resources/default_imgui_layout.ini", std::ios::binary | std::ios::trunc);
        out.write(ini_data, std::strlen(ini_data));
    }

    void loadLayoutFromFile(const std::string &path)
    {
        std::ifstream file(path);
        if (!file)
        {
            std::cerr << "[ERROR] Failed to load layout file: " << path << std::endl;
            return;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        const std::string content = buffer.str();

        ImGui::LoadIniSettingsFromMemory(content.c_str(), content.size());
    }
}
