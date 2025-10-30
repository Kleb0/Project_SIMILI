#pragma once

#include <windows.h>
// Don't include gl/GL.h - GLAD will provide OpenGL headers

class OverlayViewport {
public:
    OverlayViewport();
    ~OverlayViewport();

    bool create(HWND parent, int x, int y, int width, int height);
    void destroy();
    
    void setPosition(int x, int y, int width, int height);
    void show(bool visible);
    void render();
    
    HWND getHandle() const { return hwnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    void initializeOpenGL();
    void initializeModernRendering();
    void renderScene();
    void handleMouseInput(UINT msg, WPARAM wParam, LPARAM lParam);
    unsigned int compileShader(unsigned int type, const char* source);
    unsigned int createShaderProgram(const char* vertex_src, const char* fragment_src);
    
    HWND hwnd_;
    HWND parent_;
    HDC hdc_;
    HGLRC gl_context_;
    
    int width_;
    int height_;
    
    // Modern OpenGL resources
    unsigned int vao_;
    unsigned int vbo_;
    unsigned int ebo_;
    unsigned int shader_program_;
    
    // Background gradient
    unsigned int bg_vao_;
    unsigned int bg_vbo_;
    unsigned int bg_shader_;
    
    // Camera control
    bool mouse_dragging_;
    int last_mouse_x_;
    int last_mouse_y_;
    float camera_rotation_x_;
    float camera_rotation_y_;
    float camera_distance_;
};
