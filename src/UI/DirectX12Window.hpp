#pragma once

#include "UI/GUIWindow.hpp"
#include "imgui.h"
#include <string>

class DirectX12Window : public GUIWindow
{
public:
    std::string title = "DirectX 12 Preview";
    std::string text = "DirectX 12 is active !";

    DirectX12Window() = default;

    DirectX12Window(const std::string &title, const std::string &text)
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
