#include "Save_Scene.hpp"
#include <fstream>
#include <stdexcept>
#include <iostream>

void SaveScene::saveToJson(const std::string& filePath) const 
{
    nlohmann::json outputJson = activeScene;
    outputJson["scene_id"] = activeScene["scene_id"];

    std::ofstream file(filePath);
    if (file.is_open()) 
    {
        file << outputJson.dump(4);
        file.close();
        std::cout << "[SaveScene] Scene successfully saved to: " << std::endl;
    } 
    else 
    {
        throw std::runtime_error("Impossible d'ouvrir le fichier pour la sauvegarde.");
    }
}