#pragma once

#include "../../ThirdParty/CEF/cef_binary/include/cef_client.h"
#include "../../ThirdParty/CEF/cef_binary/include/cef_render_handler.h"
#include "../../ThirdParty/CEF/cef_binary/include/cef_life_span_handler.h"
#include "../../ThirdParty/CEF/cef_binary/include/cef_display_handler.h"
#include "TextureRendererTest.hpp"
#include <string>
#include <functional>
#include <atomic>

class HtmlTextureRenderer : public CefClient, public CefRenderHandler, public CefLifeSpanHandler, public CefDisplayHandler {
public:
    HtmlTextureRenderer(TextureRendererTest* textureRenderer);
    
    // Create the off-screen browser
    void createBrowser(const std::string& url, int width, int height);
    
    // Set the viewport window handle for forcing redraws
    void setViewportWindow(HWND viewport_hwnd) { viewport_hwnd_ = viewport_hwnd; }
    
    // Get the browser instance
    CefRefPtr<CefBrowser> getBrowser() { return browser_; }
    
    // Force browser to repaint
    void invalidate() {
        if (browser_ && browser_->GetHost()) {
            browser_->GetHost()->Invalidate(PET_VIEW);
        }
    }
    
    // Update browser dimensions and force resize
    void updateSize(int width, int height);
    
    // Send keyboard event to the off-screen browser
    void sendKeyEvent(const CefKeyEvent& event);
    
    // Set callback to be called when browser is created
    void setOnBrowserCreatedCallback(std::function<void(CefRefPtr<CefBrowser>)> callback) {
        on_browser_created_callback_ = callback;
    }
    
    // CefClient methods
    virtual CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
    virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    
    // CefRenderHandler methods
    virtual void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
    virtual void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
                        const RectList& dirtyRects, const void* buffer,
                        int width, int height) override;
    
    // CefLifeSpanHandler methods
    virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
    virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;
    
    // CefDisplayHandler methods - for console logging
    virtual bool OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                                   cef_log_severity_t level,
                                   const CefString& message,
                                   const CefString& source,
                                   int line) override;
    
    virtual void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;
    
    void detachTextureRenderer() { 
        being_destroyed_ = true;
        texture_renderer_ = nullptr; 
    }
    
    // Check if browser has been completely closed (OnBeforeClose called)
    bool isBrowserClosed() const { return browser_closed_; }
    
private:
    void injectScrollbarEliminationCSS();  // Helper method for CSS injection
    
    TextureRendererTest* texture_renderer_;
    CefRefPtr<CefBrowser> browser_;
    int width_;
    int height_;
    HWND viewport_hwnd_;
    std::function<void(CefRefPtr<CefBrowser>)> on_browser_created_callback_;
    std::atomic<bool> being_destroyed_{false};
    std::atomic<bool> browser_closed_{false};
    
    IMPLEMENT_REFCOUNTING(HtmlTextureRenderer);
};
