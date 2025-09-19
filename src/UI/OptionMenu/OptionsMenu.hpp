#pragma once
#include <imgui.h>
#include <string>
#include <functional>

class OptionsMenuContent {
public:
    bool optionsMenuHasbeenClicked = false;
    ImVec2 optionsButtonPos = ImVec2(0, 0);
    OptionsMenuContent();
    void render(const std::function<void(const std::string&)>& saveActiveSceneFunc, void* sceneRef);
};
