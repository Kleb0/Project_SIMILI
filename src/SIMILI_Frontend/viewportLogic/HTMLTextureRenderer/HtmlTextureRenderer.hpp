#pragma once

#include "../../ThirdParty/CEF/cef_binary/include/cef_client.h"
#include "../../ThirdParty/CEF/cef_binary/include/cef_render_handler.h"
#include "../../ThirdParty/CEF/cef_binary/include/cef_life_span_handler.h"
#include "../../ThirdParty/CEF/cef_binary/include/cef_display_handler.h"
#include <glad/glad.h>
#include <string>
#include <functional>
#include <atomic>

class HtmlTextureRenderer : public CefClient, public CefRenderHandler, public CefLifeSpanHandler, public CefDisplayHandler {
public:
    HtmlTextureRenderer();
    ~HtmlTextureRenderer();
    
    // Initialize OpenGL resources
    void initialize(int width, int height);
    
    // Cleanup OpenGL resources
    void cleanup();
    
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
    
    // Render the texture
    void render();
    
    // Resize viewport
    void resize(int width, int height);
    
    // Set render rectangle
    void setRenderRect(int x, int y, int w, int h);
    
    // Get texture ID
    GLuint getTextureId() const { return texture_id_; }
    
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
    
    // Check if browser has been completely closed (OnBeforeClose called)
    bool isBrowserClosed() const { return browser_closed_; }
    
private:
    void injectScrollbarEliminationCSS();
    void updateTexture(const void* buffer, int width, int height);
    void createTexture(int tex_width, int tex_height);
    void createQuadMesh();
    void createShaderProgram();
    GLuint compileShader(GLenum type, const char* source);
    
    // OpenGL resources
    GLuint texture_id_;
    GLuint vao_;
    GLuint vbo_;
    GLuint shader_program_;
    
    int width_;
    int height_;
    int texture_width_;
    int texture_height_;
    bool initialized_;
    
    int render_x_;
    int render_y_;
    int render_width_;
    int render_height_;
    
    // CEF browser
    CefRefPtr<CefBrowser> browser_;
    HWND viewport_hwnd_;
    std::function<void(CefRefPtr<CefBrowser>)> on_browser_created_callback_;
    std::atomic<bool> being_destroyed_{false};
    std::atomic<bool> browser_closed_{false};
    
    IMPLEMENT_REFCOUNTING(HtmlTextureRenderer);
};
