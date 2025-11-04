#pragma once

#include <windows.h>

// Forward declarations
class ThreeDScene;

class OverlayViewport {
public:
    OverlayViewport();
    ~OverlayViewport();

    bool create(HWND parent, int x, int y, int width, int height);
    void destroy();
    
    void setPosition(int x, int y, int width, int height);
    void show(bool visible);
    bool isVisible() const;
    void render();
    
    // Control rendering
    void enableRendering(bool enable) { rendering_enabled_ = enable; }
    bool isRenderingEnabled() const { return rendering_enabled_; }
    
    // 3D Scene management
    void setThreeDScene(ThreeDScene* scene) { three_d_scene_ = scene; }
    ThreeDScene* getThreeDScene() const { return three_d_scene_; }
    
    // OpenGL context management
    void makeContextCurrent();
    void releaseContext();
    
    HWND getHandle() const { return hwnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    void initializeOpenGL();
    void renderScene();
    
    HWND hwnd_;
    HWND parent_;
    HDC hdc_;
    HGLRC gl_context_;
    
    int width_;
    int height_;
    
    ThreeDScene* three_d_scene_;  // Non-owning pointer to 3D scene
    
    // Rendering control
    bool rendering_enabled_;
};

