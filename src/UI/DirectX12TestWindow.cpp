#include "UI/DirectX12TestWindow.hpp"

DirectX12TestWindow::DirectX12TestWindow()
{
    InitWindow();
    renderer = std::make_unique<DirectX12Renderer>(hwnd);
}

DirectX12TestWindow::~DirectX12TestWindow()
{
    DestroyWindow(hwnd);
}

std::unique_ptr<DirectX12Renderer> &DirectX12TestWindow::getRenderer()
{
    return renderer;
}

void DirectX12TestWindow::InitWindow()
{
    const wchar_t CLASS_NAME[] = L"DirectX12WindowClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClass(&wc);

    hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"DirectX 12 Preview",
        WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr, nullptr, wc.hInstance, nullptr);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
}

void DirectX12TestWindow::Render()
{
    if (renderer && IsWindow(hwnd))
    {
        renderer->Render();
    }
}

LRESULT CALLBACK DirectX12TestWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

void DirectX12TestWindow::render()
{
    Render();
}
