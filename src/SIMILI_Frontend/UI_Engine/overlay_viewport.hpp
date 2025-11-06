#pragma once

#include <windows.h>
#include <vector>

// Forward declarations
namespace ImGuizmo {
    enum OPERATION;
    enum MODE;
}

class ThreeDScene;
class ThreeDObject;
class ThreeDObjectSelector;
class Camera;

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
    
    // Raycast & Selection
    void performRaycast(int mouseX, int mouseY);
    
    // Guizmo management
    void setGuizmoOperation(ImGuizmo::OPERATION operation) { current_guizmo_operation_ = operation; }
    ImGuizmo::OPERATION getGuizmoOperation() const { return current_guizmo_operation_; }
    void setGuizmoMode(ImGuizmo::MODE mode) { current_guizmo_mode_ = mode; }
    ImGuizmo::MODE getGuizmoMode() const { return current_guizmo_mode_; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    void initializeOpenGL();
    void initializeImGui();
    void shutdownImGui();
    void renderScene();
    void renderGuizmo();
    
    HWND hwnd_;
    HWND parent_;
    HDC hdc_;
    HGLRC gl_context_;
    
    int width_;
    int height_;
    
    ThreeDScene* three_d_scene_;      
    bool rendering_enabled_;    
    bool is_dragging_;
    POINT last_mouse_pos_;
    bool imgui_initialized_;
    
    // Raycast & Selection
    ThreeDObjectSelector* selector_;
    
    // Guizmo state
    ImGuizmo::OPERATION current_guizmo_operation_;
    ImGuizmo::MODE current_guizmo_mode_;
};

