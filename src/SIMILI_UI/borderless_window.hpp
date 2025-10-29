// borderless_window.hpp - Fenêtre sans bordures pour CEF
#pragma once

#include <windows.h>
#include <string>

class BorderlessWindow {
public:
    BorderlessWindow(const std::string& title, int width, int height, int x, int y)
        : hwnd_(nullptr), width_(width), height_(height), x_(x), y_(y), title_(title) {}

    ~BorderlessWindow() {
        if (hwnd_) {
            DestroyWindow(hwnd_);
        }
    }

    HWND create() {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"SIMILIBorderlessWindow";

        RegisterClassExW(&wc);

        DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME;
        
        std::wstring wtitle(title_.begin(), title_.end());

        hwnd_ = CreateWindowExW(
            0,
            L"SIMILIBorderlessWindow",
            wtitle.c_str(),
            style,
            x_, y_, width_, height_,
            nullptr,
            nullptr,
            GetModuleHandle(nullptr),
            nullptr
        );

        if (hwnd_) {
            ShowWindow(hwnd_, SW_SHOW);
            UpdateWindow(hwnd_);
        }

        return hwnd_;
    }

    HWND getHandle() const { return hwnd_; }

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_CLOSE:
                PostQuitMessage(0);
                return 0;
            case WM_DESTROY:
                return 0;
            default:
                return DefWindowProcW(hwnd, uMsg, wParam, lParam);
        }
    }

    HWND hwnd_;
    int width_;
    int height_;
    int x_;
    int y_;
    std::string title_;
};
