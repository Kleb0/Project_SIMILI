#pragma once
#include <Windows.h>
#include <memory>
#include "ThirdParty/DirectX12/DirectX12Renderer.hpp"

class DirectX12TestWindow
{
public:
    DirectX12TestWindow();
    ~DirectX12TestWindow();

    void Render();

private:
    HWND hwnd;
    std::unique_ptr<DirectX12Renderer> renderer;

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void InitWindow();
};
