#pragma once

#include <windows.h>
#include <vector>
#include <list>
#include <mutex>
#include <atomic>

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

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
class ContextualMenuTextureTest;
class UIHandler;
class SlotTexture;

class OverlayViewport {
public:
    // ----------- Lifecycle Management -----------
    OverlayViewport();
    ~OverlayViewport();

    bool create(HWND parent, int x, int y, int width, int height);
    void destroy();
    
    // ----------- Window Management -----------
    void setPosition(int x, int y, int width, int height);
    void show(bool visible);
    bool isVisible() const;
    void ensureProperZOrder();
    void setZOrderLayer(int layer) { z_order_layer_ = layer; ensureProperZOrder(); }
    int getZOrderLayer() const { return z_order_layer_; }
    
    HWND getHandle() const { return hwnd_; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    
    // ----------- Rendering Control -----------
    void render();
    void enableRendering(bool enable) { rendering_enabled_ = enable; }
    bool isRenderingEnabled() const { return rendering_enabled_; }
    
    // ----------- OpenGL Context Management -----------
    void makeContextCurrent();
    void releaseContext();
    HGLRC getGLContext() const { return gl_context_; }
    
    // ----------- Mesh OpenGL Resource Management -----------
    void reinitializeMeshComponents(class Mesh* mesh);
    
    // ----------- 3D Scene Management -----------
    void setThreeDScene(ThreeDScene* scene) { three_d_scene_ = scene; }
    ThreeDScene* getThreeDScene() const { return three_d_scene_; }
    
    // ----------- Raycast & Object Selection -----------
    void performRaycast(int mouseX, int mouseY);
    ThreeDObjectSelector* getSelector() { return selector_; }
    
    void setMultipleSelectedObjects(const std::list<ThreeDObject*>& objects);
    const std::list<ThreeDObject*>& getMultipleSelectedObjects() const { return multiple_selected_objects_; }
    
    void setUIHandler(UIHandler* handler) { ui_handler_ = handler; }
    
    std::list<Vertice*>& getMultipleSelectedVertices() { return multiple_selected_vertices_; }
    std::list<Face*>& getMultipleSelectedFaces() { return multiple_selected_faces_; }
    std::list<Edge*>& getMultipleSelectedEdges() { return multiple_selected_edges_; }
    
    // ----------- Gizmo Management -----------
    void setGuizmoOperation(ImGuizmo::OPERATION operation) { current_guizmo_operation_ = operation; }
    ImGuizmo::OPERATION getGuizmoOperation() const { return current_guizmo_operation_; }
    void setGuizmoMode(ImGuizmo::MODE mode) { current_guizmo_mode_ = mode; }
    ImGuizmo::MODE getGuizmoMode() const { return current_guizmo_mode_; }
    
    // ----------- 3D Modeling Mode Management -----------
    void setModelingMode(ThreeDMode* mode);
    void switchModeByKey(int keyNumber);
    ThreeDMode* getCurrentMode() const { return current_mode_; }
    
    Normal_Mode* getNormalMode() { return normal_mode_; }
    Vertice_Mode* getVerticeMode() { return vertice_mode_; }
    Face_Mode* getFaceMode() { return face_mode_; }
    Edge_Mode* getEdgeMode() { return edge_mode_; }
    
    // ----------- HTML Texture Rendering -----------
    HtmlTextureRenderer* getHtmlTextureRenderer() const { return html_texture_renderer_; }
    
    // ----------- Contextual Menu Management -----------
    HtmlTextureRenderer* getContextualMenuRenderer() const { return contextual_menu_html_renderer_; }
    void showContextualMenu(bool show) { contextual_menu_visible_ = show; }
    void hideContextualMenu() { contextual_menu_visible_ = false; }
    bool isContextualMenuVisible() const { return contextual_menu_visible_; }
    void setContextualMenuPosition(int x, int y);
    
    // ----------- Slot Texture Management -----------
    void createSlotTexture(int x, int y, int width, int height);
    void destroySlotTexture();
    void showSlotTexture(bool visible);
    SlotTexture* getSlotTexture() const { return slot_texture_; }
    
    // ----- Manipulation in scene -----
    void MoveCameraLaterally(int deltaX, int deltaY);
    void ProcessWheelInput(int wheelDirection);
    void ProcessZoom(int wheelDirection);
    
    bool isEdgeLoopActive = false;

private:
    // ----------- Windows Callback -----------
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    // ----------- Initialization & Cleanup -----------
    void initializeOpenGL(HGLRC shareContext = nullptr);
    void initializeImGui();
    void shutdownImGui();
    
    // ----------- Rendering Internal -----------
    void renderScene();
    void ThreeDWorldInteractions();
    void update_Scene_Rendering();
    
    // ----------- Window & OpenGL Context -----------
    HWND hwnd_;
    HWND parent_;
    HDC hdc_;
    HGLRC gl_context_;
    
    int width_;
    int height_;
    int z_order_layer_ = 1;  // Layer position (higher = on top)
    
    bool rendering_enabled_;
    bool imgui_initialized_;
    
    // ----------- 3D Scene -----------
    ThreeDScene* three_d_scene_;
    UIHandler* ui_handler_ = nullptr;
    
    // ----------- Pending Meshes (Thread-Safe Queue) -----------
    std::vector<class Mesh*> pending_meshes_to_finalize_;
    std::mutex pending_meshes_mutex_;
    std::atomic<bool> has_pending_meshes_{false};
    
    // ----------- Selection System -----------
    ThreeDObjectSelector* selector_;
    std::list<ThreeDObject*> multiple_selected_objects_;
    std::list<Vertice*> multiple_selected_vertices_;
    std::list<Face*> multiple_selected_faces_;
    std::list<Edge*> multiple_selected_edges_;
    
    // ----------- Interaction Controllers -----------
    CameraControl* camera_control_;
    RaycastPerform* raycast_performer_;
    OverlayClickHandler* click_handler_;
    
    // ----------- Gizmo State -----------
    ImGuizmo::OPERATION current_guizmo_operation_;
    ImGuizmo::MODE current_guizmo_mode_;
    bool was_using_gizmo_last_frame_;
    
    // ----------- 3D Modeling Modes -----------
    Normal_Mode* normal_mode_;
    Vertice_Mode* vertice_mode_;
    Face_Mode* face_mode_;
    Edge_Mode* edge_mode_;
    ThreeDMode* current_mode_;
    
    // ----------- HTML Texture Rendering -----------
    TextureRendererTest* texture_renderer_test_;
    HtmlTextureRenderer* html_texture_renderer_;
    
    int html_texture_x_ = 10;
    int html_texture_y_ = 10;
    int html_texture_width_ = 350;
    int html_texture_height_ = 100;
    
    // ----------- Contextual Menu Texture -----------
    ContextualMenuTextureTest* contextual_menu_texture_test_;
    
    // ----------- Contextual Menu HTML Rendering -----------
    TextureRendererTest* contextual_menu_texture_renderer_;
    HtmlTextureRenderer* contextual_menu_html_renderer_;
    
    int contextual_menu_x_ = 100;
    int contextual_menu_y_ = 100;
    int contextual_menu_width_ = 300;
    int contextual_menu_height_ = 200;
    bool contextual_menu_visible_ = false;
    
    // ----------- Slot Texture (Layer 2) -----------
    SlotTexture* slot_texture_;
};