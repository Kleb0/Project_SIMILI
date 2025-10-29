#include "ui_process_manager.hpp"
#include "ThreeDScene.hpp"
#include <json.hpp>

void UIProcessManager::sendObjectsListToUI()
{
    if (!scene_) {
        std::cerr << "[UIProcessManager] No scene set, cannot send objects list\n";
        return;
    }

    nlohmann::json message;
    message["type"] = "objects_list";
    message["data"] = scene_->getObjectsListAsJson();

    std::string jsonStr = message.dump();
    
    std::cout << "[UIProcessManager] Sending objects list to UI: " << jsonStr << "\n";
    
    sendToUI(jsonStr);
}
