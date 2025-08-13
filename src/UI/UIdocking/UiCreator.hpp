#pragma once
#include <string>

namespace UiCreator
{
    std::string getLayoutFilePath(const std::string &filename);
    std::string loadLastLayoutPath();
    void saveCurrentLayoutToDefault();
    void loadLayoutFromFile(const std::string &path);
    void saveLastLayoutPath(const std::string &path);

}