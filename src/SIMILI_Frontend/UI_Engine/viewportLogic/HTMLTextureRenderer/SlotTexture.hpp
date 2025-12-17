#pragma once

#include <windows.h>
#include <glad/glad.h>

// Forward declarations for CEF integration
class HtmlTextureRenderer;
class TextureRendererTest;



class SlotTexture {
public:
    SlotTexture();
    ~SlotTexture();

    // ----------- Window Management -----------
    bool create(HWND parent, int x, int y, int width, int height, int zOrderLayer = 2);
    void destroy();
    void setPosition(int x, int y, int width, int height);
    void show(bool visible);
    bool isVisible() const;
    
    // ----------- Z-Order Management -----------
    void setZOrderLayer(int layer);
    int getZOrderLayer() const { return z_order_layer_; }
    void ensureProperZOrder();
    
    // ----------- Rendering -----------
    void render();
    void setColor(float r, float g, float b, float a = 1.0f);
    
    // ----------- CEF HTML Rendering -----------
    void loadHTML(const std::string& url);
    void setUseHTMLTexture(bool useTexture);
    bool isUsingHTMLTexture() const { return use_html_texture_; }
    void updateHTMLTextureSize(int width, int height);  // Synchronize CEF browser size
    

    // ----------- Accessors -----------
    HWND getHandle() const { return hwnd_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    void initializeOpenGL(HGLRC shareContext = nullptr);
    void createQuad();
    void renderQuad();
    void onBrowserCreated(class CefBrowser* browser);
    
    // ----------- Window & OpenGL -----------
    HWND hwnd_;
    HWND parent_;
    HDC hdc_;
    HGLRC gl_context_;
    
    int width_;
    int height_;
    int z_order_layer_;
    
    // ----------- OpenGL Resources -----------
    GLuint vao_;
    GLuint vbo_;
    GLuint shader_program_;
    
    // ----------- Color -----------
    float color_r_;
    float color_g_;
    float color_b_;
    float color_a_;
    
    // ----------- CEF HTML Rendering -----------
    HtmlTextureRenderer* html_renderer_;
    TextureRendererTest* texture_renderer_;
    bool use_html_texture_;
    bool html_browser_ready_;
};
