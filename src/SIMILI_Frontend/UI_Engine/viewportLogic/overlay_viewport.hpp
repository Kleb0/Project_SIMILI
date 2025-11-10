#pragma once

#include <windows.h>
#include <vector>
#include <list>

// Forward declarations
namespace ImGuizmo {
    enum OPERATION;
    enum MODE;
}

class ThreeDScene;
class ThreeDObject;
class ThreeDObjectSelector;
class Camera;
class CameraControl;
class RaycastPerform;
class TextureRendererTest;
class HtmlTextureRenderer;
class ThreeDMode;
class Normal_Mode;
class Vertice_Mode;
class Face_Mode;
class Edge_Mode;
class OverlayClickHandler;
class Vertice;
class Face;
class Edge;

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
    
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    
    // Raycast & Selection
    void performRaycast(int mouseX, int mouseY);
    
    // Guizmo management
    void setGuizmoOperation(ImGuizmo::OPERATION operation) { current_guizmo_operation_ = operation; }
    ImGuizmo::OPERATION getGuizmoOperation() const { return current_guizmo_operation_; }
    void setGuizmoMode(ImGuizmo::MODE mode) { current_guizmo_mode_ = mode; }
    ImGuizmo::MODE getGuizmoMode() const { return current_guizmo_mode_; }
    
    // HTML texture access
    HtmlTextureRenderer* getHtmlTextureRenderer() const { return html_texture_renderer_; }
    
    // 3D Mode management
    void setModelingMode(ThreeDMode* mode);
    void switchModeByKey(int keyNumber);
    ThreeDMode* getCurrentMode() const { return current_mode_; }
    
    // Access to individual modes
    Normal_Mode* getNormalMode() { return normal_mode_; }
    Vertice_Mode* getVerticeMode() { return vertice_mode_; }
    Face_Mode* getFaceMode() { return face_mode_; }
    Edge_Mode* getEdgeMode() { return edge_mode_; }
    
    // Object selection management
    void setMultipleSelectedObjects(const std::list<ThreeDObject*>& objects);
    const std::list<ThreeDObject*>& getMultipleSelectedObjects() const { return multiple_selected_objects_; }
    
    // Selection lists for different modes
    std::list<Vertice*>& getMultipleSelectedVertices() { return multiple_selected_vertices_; }
    std::list<Face*>& getMultipleSelectedFaces() { return multiple_selected_faces_; }
    std::list<Edge*>& getMultipleSelectedEdges() { return multiple_selected_edges_; }
    
    // Selector access
    ThreeDObjectSelector* getSelector() { return selector_; }

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
    
    bool imgui_initialized_;
    
    // Raycast & Selection
    ThreeDObjectSelector* selector_;
    
    // Guizmo state
    ImGuizmo::OPERATION current_guizmo_operation_;
    ImGuizmo::MODE current_guizmo_mode_;
    class CameraControl* camera_control_;
    class RaycastPerform* raycast_performer_;
    
    // Red texture overlay test
    TextureRendererTest* texture_renderer_test_;
    HtmlTextureRenderer* html_texture_renderer_;
    
    // HTML texture dimensions and position
    int html_texture_x_ = 10;
    int html_texture_y_ = 10;
    int html_texture_width_ = 350;
    int html_texture_height_ = 100;
    
    Normal_Mode* normal_mode_;
    Vertice_Mode* vertice_mode_;
    Face_Mode* face_mode_;
    Edge_Mode* edge_mode_;
    ThreeDMode* current_mode_;
    
    std::list<ThreeDObject*> multiple_selected_objects_;
    std::list<Vertice*> multiple_selected_vertices_;
    std::list<Face*> multiple_selected_faces_;
    std::list<Edge*> multiple_selected_edges_;
    bool was_using_gizmo_last_frame_;
    
    OverlayClickHandler* click_handler_;
    
public:
    bool isEdgeLoopActive = false;
    
private:
    void ThreeDWorldInteractions();
};

