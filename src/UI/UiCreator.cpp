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
            std::cerr << "[UI_CREATOR_WARNING] Layout file not found at: " << path << std::endl;
            std::cout << "[UI_CREATOR_INFO] Attempting to copy layout file from source folder..." << std::endl;
            fs::path fallbackPath = fs::current_path().parent_path().parent_path() / "src" / "resources" / fs::path(path).filename();

            if (fs::exists(fallbackPath))
            {
                fs::create_directories(fs::path(path).parent_path());
                try
                {
                    fs::copy_file(fallbackPath, path, fs::copy_options::overwrite_existing);
                    std::cout << "[UI_CREATOR_INFO] Successfully copied layout file from: " << fallbackPath << " to: " << path << std::endl;
                }
                catch (const fs::filesystem_error &e)
                {
                    std::cerr << "[ERROR] Failed to copy layout file: " << e.what() << std::endl;
                    return;
                }

                file.open(path);
                if (!file)
                {
                    std::cerr << "[ERROR] Failed to open copied layout file: " << path << std::endl;
                    return;
                }
            }

            else
            {
                std::cerr << "[ERROR] Layout not found in source folder either: " << fallbackPath << std::endl;
                return;
            }
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        const std::string content = buffer.str();
        ImGui::LoadIniSettingsFromMemory(content.c_str(), content.size());
    }
}
