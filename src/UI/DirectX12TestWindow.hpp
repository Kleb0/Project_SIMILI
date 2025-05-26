#pragma once
#include "UI/GUIWindow.hpp"
#include <Windows.h>
#include <memory>
#include "ThirdParty/DirectX12/DirectX12Renderer.hpp"
#include <string>

class DirectX12TestWindow : public GUIWindow
{
public:
    DirectX12TestWindow();
    ~DirectX12TestWindow();
    std::unique_ptr<DirectX12Renderer> &getRenderer();
    void Render();
    void render() override;

    std::string title;
    std::string text;

private:
    HWND hwnd;
    std::unique_ptr<DirectX12Renderer> renderer;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void InitWindow();
};
