// borderless_window.hpp - Fenêtre sans bordures pour CEF
#pragma once

#include <windows.h>
#include <string>
#include "include/cef_browser.h"

class BorderlessWindow {
public:
    BorderlessWindow(const std::string& title, int width, int height, int x, int y);
    ~BorderlessWindow();
    
    void setBrowser(CefRefPtr<CefBrowser> browser);
    HWND create();
    HWND getHandle() const;
    
    static BorderlessWindow* GetInstance();
    static void SetInstance(BorderlessWindow* instance);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    HWND hwnd_;
    int width_;
    int height_;
    int x_;
    int y_;
    std::string title_;
    CefRefPtr<CefBrowser> browser_;
};
