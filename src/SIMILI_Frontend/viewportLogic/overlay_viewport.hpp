#pragma once

#include <SDL3/SDL.h>
#include <vector>
#include <list>
#include <mutex>
#include <atomic>

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

namespace ImGuizmo 
{
	enum OPERATION;
	enum MODE;
}

class VKScene;
class ThreeDObject;
class ThreeDObjectSelector;
class Camera;
class CameraControl;
class RaycastPerform;
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
class UIHandler;
class Mesh;

class OverlayViewport 
{
	public:
		// ----------- Lifecycle Management -----------
		OverlayViewport();
		~OverlayViewport();

		bool create(SDL_Window* parent, int x, int y, int width, int height);
		void destroy();
		
		// ----------- Window Management -----------
		void setPosition(int x, int y, int width, int height);
		void show(bool visible);
		bool isVisible() const;
		
		SDL_Window* getHandle() const { return sdl_window_; }
		int getWidth() const { return width_; }
		int getHeight() const { return height_; }
		
		void updateViewportDimensions(int width, int height);
		
		// ----------- Rendering Control -----------
		void render();
		// handle of of SDL3 events (keyboard, window resize, etc.)
		void handleEvents(); 
		void enableRendering(bool enable) { rendering_enabled_ = enable; }
		bool isRenderingEnabled() const { return rendering_enabled_; }
		
		// ----------- OpenGL Context Management -----------
		void makeContextCurrent();
		void releaseContext();
		SDL_GLContext getGLContext() const { return gl_context_; }
		
		// ----------- 3D Scene Management -----------
		void setVKScene(VKScene* scene) { vk_scene_ = scene; }
		VKScene* getVKScene() const { return vk_scene_; }
		
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
		bool isGizmoActive() const { return is_gizmo_active_; }
		
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
		
		// ----------- Mesh Management -----------
		void reinitializeMeshComponents(Mesh* mesh);
		
		// ----- Manipulation in scene -----
		void MoveCameraLaterally(int deltaX, int deltaY);
		void ProcessWheelInput(int wheelDirection);
		void ProcessZoom(int wheelDirection);
		void ProcessMouseMovementWhileLeftClicking(int deltaX, int deltaY);
		void ProcessCameraOrbiting(int deltaX, int deltaY);
		void shootRaycastFromUIHandler(int mouseX, int mouseY);
		void injectMouseInputs(int mouseX, int mouseY, bool leftDown, bool rightDown, bool middleDown, float wheelDelta);
		
		// ----------- Initialization & Cleanup -----------
		void initializeOpenGL(SDL_GLContext shareContext = nullptr);
		void initializeImGui();
		void shutdownImGui();
		
		// ----------- Rendering Internal -----------
		void renderScene();
		void ThreeDWorldInteractions();
		void update_Scene_Rendering();
		
		// ----------- Window & OpenGL Context -----------
		SDL_Window* sdl_window_;
		SDL_Window* parent_window_;
		SDL_GLContext gl_context_;
		int width_;
		int height_;
		bool rendering_enabled_;
		bool imgui_initialized_;
		
		// ----------- 3D Scene -----------
		VKScene* vk_scene_;
		UIHandler* ui_handler_ = nullptr;
		
		// ----------- Pending Meshes (Thread-Safe Queue) -----------
		std::vector<Mesh*> pending_meshes_to_finalize_;
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
		bool is_gizmo_active_;
		
		// ----------- 3D Modeling Modes -----------
		Normal_Mode* normal_mode_;
		Vertice_Mode* vertice_mode_;
		Face_Mode* face_mode_;
		Edge_Mode* edge_mode_;
		ThreeDMode* current_mode_;
		
		// ----------- HTML Texture Rendering -----------
		HtmlTextureRenderer* html_texture_renderer_;
		
		int html_texture_x_ = 10;
		int html_texture_y_ = 10;
		int html_texture_width_ = 350;
		int html_texture_height_ = 100;
		
		int injected_mouse_x_ = 0;
		int injected_mouse_y_ = 0;
		bool injected_left_down_ = false;
		bool injected_right_down_ = false;
		bool injected_middle_down_ = false;
		float injected_wheel_delta_ = 0.0f;
		bool has_injected_inputs_ = false;
};