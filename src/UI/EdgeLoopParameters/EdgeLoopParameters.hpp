#pragma once
#include <imgui.h>
#include "Engine/ThreeDScene.hpp"

class ThreeDScene;
class EdgeLoopParameters 
{
public:

    void render();

    void setScene(ThreeDScene* scene);
    ThreeDScene* getScene() const { return scene; }

private:
    bool isOpen = false;
    ImVec2 popupPos = ImVec2(0, 0);
    ThreeDScene* scene = nullptr;
};
