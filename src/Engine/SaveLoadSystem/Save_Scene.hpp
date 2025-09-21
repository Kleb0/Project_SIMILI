#ifndef SAVE_SCENE_HPP
#define SAVE_SCENE_HPP

#include <string>
#include "ThirdParty/json.hpp"

class SaveScene 
{


public:
    SaveScene(const nlohmann::json& scene) : activeScene(scene) {}

    void setActiveScene(const nlohmann::json& scene) 
    {
        activeScene = scene;
    }

    const nlohmann::json& getActiveScene() const 
    {
        return activeScene;
    }

    void saveSceneToJson(const class ThreeDScene_DNA* dna, const std::string& filePath);

private:
    nlohmann::json activeScene;

};

#endif 