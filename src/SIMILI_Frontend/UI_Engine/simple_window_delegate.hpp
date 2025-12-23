#pragma once

#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/base/cef_callback.h"
#include <iostream>

class UIHandler;

class SimpleWindowDelegate : public CefWindowDelegate {
public:
    explicit SimpleWindowDelegate(CefRefPtr<CefBrowserView> browser_view);

    void OnWindowCreated(CefRefPtr<CefWindow> window) override;
    void OnWindowDestroyed(CefRefPtr<CefWindow> window) override;
    bool CanClose(CefRefPtr<CefWindow> window) override;
    CefSize GetPreferredSize(CefRefPtr<CefView> view) override;

    // New methods for maximized state detection
    bool isWindowMaximized() const;
    void getMaximizedBorderOffsets(int& offsetX, int& offsetY, int& offsetWidth, int& offsetHeight) const;
    HWND getWindowHandle() const { return window_hwnd_; }
    
    void checkAndCaptureIfMaximized();
    void setUIHandler(UIHandler* handler) { ui_handler_ = handler; }

private:
    CefRefPtr<CefBrowserView> browser_view_;
    HWND window_hwnd_;
    UIHandler* ui_handler_;
    bool maximization_captured_;

    IMPLEMENT_REFCOUNTING(SimpleWindowDelegate);
    DISALLOW_COPY_AND_ASSIGN(SimpleWindowDelegate);
};
