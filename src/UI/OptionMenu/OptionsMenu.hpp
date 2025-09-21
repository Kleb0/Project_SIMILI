#pragma once
#include <imgui.h>
#include <string>
#include <functional>


class ThreeDScene;

class OptionsMenuContent {
public:
    bool optionsMenuHasbeenClicked = false;
    ImVec2 optionsButtonPos = ImVec2(0, 0);
    OptionsMenuContent();

    void setScene(ThreeDScene* scene) { sceneRef = scene; }
    ThreeDScene* getScene() const { return sceneRef; }

    void render(void* sceneRef);

private:
    ThreeDScene* sceneRef = nullptr;
};
