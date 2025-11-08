#pragma once

#include "../../ThirdParty/CEF/cef_binary/include/cef_client.h"
#include "../../ThirdParty/CEF/cef_binary/include/cef_render_handler.h"
#include "../../ThirdParty/CEF/cef_binary/include/cef_life_span_handler.h"
#include "../../ThirdParty/CEF/cef_binary/include/cef_display_handler.h"
#include "TextureRendererTest.hpp"
#include <string>

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
    
    // Send keyboard event to the off-screen browser
    void sendKeyEvent(const CefKeyEvent& event);
    
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
    
private:
    TextureRendererTest* texture_renderer_;
    CefRefPtr<CefBrowser> browser_;
    int width_;
    int height_;
    HWND viewport_hwnd_;
    
    IMPLEMENT_REFCOUNTING(HtmlTextureRenderer);
};
