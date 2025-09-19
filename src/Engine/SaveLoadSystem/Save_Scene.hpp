#ifndef SAVE_SCENE_HPP
#define SAVE_SCENE_HPP

#include <string>
#include "ThirdParty/json.hpp"

class SaveScene {
private:
    nlohmann::json activeScene;

public:
    SaveScene(const nlohmann::json& scene) : activeScene(scene) {}

    void setActiveScene(const nlohmann::json& scene) {
        activeScene = scene;
    }

    const nlohmann::json& getActiveScene() const {
        return activeScene;
    }

    void saveToJson(const std::string& filePath) const;
};

#endif // SAVE_SCENE_HPP