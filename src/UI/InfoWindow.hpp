#pragma once

#include "UI/GUIWindow.hpp"
#include "imgui.h"
#include <string>

class InfoWindow : public GUIWindow
{
public:
    std::string title = "Hello Window";
    std::string text = "Bienvenue dans la première fenêtre !";

    InfoWindow() = default;

    InfoWindow(const std::string &title, const std::string &text)
        : title(title), text(text)
    {
    }

    void render() override
    {
        ImGui::Begin(title.c_str());
        ImGui::Text("%s", text.c_str());
        ImGui::End();
    }
};
