#pragma once
#include <string>

namespace UiCreator
{
    std::string getLayoutFilePath(const std::string &filename);
    void saveCurrentLayoutToDefault();
    void loadLayoutFromFile(const std::string &path);
}