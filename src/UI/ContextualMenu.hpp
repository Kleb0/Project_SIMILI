#pragma once

#include <imgui.h>

class ContextualMenu
{
public:
    void show();
    void hide();
    void render();

private:
    bool isOpen = false;
    ImVec2 popupPos = ImVec2(0, 0);
};