#pragma once

#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/base/cef_callback.h"
#include <iostream>
#include <string>
#include <map>
#include <mutex>

class UIHandler;

struct IFrameData
{
	std::string name;
	int x;
	int y;
	int width;
	int height;
	int clientX;
	int clientY;
};

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
    
    void checkAndCaptureWindowStateChange();
    void setUIHandler(UIHandler* handler) { ui_handler_ = handler; }
    
    // IFrame management methods
    void updateIFrameData(const std::string& name, int x, int y, int width, int height, int clientX = 0, int clientY = 0);
    bool getIFrameData(const std::string& name, IFrameData& outData) const;
    std::map<std::string, IFrameData> getAllIFrames() const;
    void clearAllIFrames();
    
    // Window movement detection
    static LRESULT CALLBACK WindowSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

private:
    CefRefPtr<CefBrowserView> browser_view_;
    HWND window_hwnd_;
    UIHandler* ui_handler_;
    bool last_maximized_state_;
    int last_window_x_;
    int last_window_y_;
    
    // IFrame data storage
    mutable std::mutex iframe_mutex_;
    std::map<std::string, IFrameData> iframeDataMap_;

    IMPLEMENT_REFCOUNTING(SimpleWindowDelegate);
    DISALLOW_COPY_AND_ASSIGN(SimpleWindowDelegate);
};
