#include <iostream>
#include <glad/glad.h>

// ImGui MUST be included BEFORE ImGuizmo
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <ImGuizmo.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include "overlay_viewport.hpp"
#include "CameraControl/CameraControl.hpp"
#include "Raycasting/RaycastPerform.hpp"
#include "HTMLTextureRenderer/HtmlTextureRenderer.hpp"
#include "ClickHandling/OverlayClickHandler.hpp"

#include "../../Engine/VulkanScene/VKScene.Hpp"
#include "../../Engine/VulkanScene/VKcontext.hpp"
#include "../../Engine/ThreeDObjectSelector.hpp"
#include "../../Engine/ThreeDInteractions/MeshTransform.hpp"
#include "../../Engine/Guizmo.hpp"
#include "../../WorldObjects/Camera/Camera.hpp"
#include "../../WorldObjects/Entities/ThreeDObject.hpp"
#include "../../WorldObjects/Mesh/Mesh.hpp"
#include "../../WorldObjects/Basic/Vertice.hpp"
#include "../../WorldObjects/Basic/Edge.hpp"
#include "../../WorldObjects/Basic/Face.hpp"
#include "../../ThirdParty/CEF/cef_binary/include/cef_app.h"
#include "../../../Engine/ThreeDModes/ThreeDMode.hpp"
#include "../../../Engine/ThreeDModes/Normal_Mode.hpp"
#include "../../../Engine/ThreeDModes/Vertice_Mode.hpp"
#include "../../../Engine/ThreeDModes/Face_Mode.hpp"
#include "../../../Engine/ThreeDModes/Edge_Mode.hpp"
#include "../../Engine/ThreeDInteractions/MeshTransform.hpp"
#include "../../Engine/ThreeDInteractions/VerticeTransform.hpp"
#include "../../Engine/ThreeDInteractions/FaceTransform.hpp"
#include "../../Engine/ThreeDInteractions/EdgeTransform.hpp"
#include "Keymanagement/KeyManager.hpp"
#include "Keymanagement/MouseControlToOverlay.hpp"
#include "FrameDatas/FrameDatas.hpp"
#include "../ui_handler.hpp"
// ============================================================================
// LIFECYCLE MANAGEMENT
// ============================================================================


OverlayViewport::OverlayViewport() : sdl_window_(nullptr) 
	, parent_window_(nullptr)
	, gl_context_(nullptr)
	, width_(800)
	, height_(600)
	, vk_scene_(nullptr)
	, rendering_enabled_(true)
	, imgui_initialized_(false)
	, selector_(nullptr)
	, current_guizmo_operation_(ImGuizmo::TRANSLATE)
	, current_guizmo_mode_(ImGuizmo::LOCAL)
	, html_texture_renderer_(nullptr)
	, normal_mode_(nullptr)
	, vertice_mode_(nullptr)
	, face_mode_(nullptr)
	, edge_mode_(nullptr)
	, current_mode_(nullptr)
	, was_using_gizmo_last_frame_(false)
{
	selector_ = new ThreeDObjectSelector();
	camera_control_ = new CameraControl(this);
	raycast_performer_ = new RaycastPerform(this, selector_);
	click_handler_ = new OverlayClickHandler(this);
	
	normal_mode_ = new Normal_Mode();
	vertice_mode_ = new Vertice_Mode();
	face_mode_ = new Face_Mode();
	edge_mode_ = new Edge_Mode();
	
	current_mode_ = normal_mode_;
	
	SIMILI::Input::KeyManager::getInstance().initialize();
	
	SIMILI::Input::KeyManager::getInstance().bindGizmoActions(
		[this]() { this->setGuizmoOperation(ImGuizmo::TRANSLATE); std::cout << "[OverlayViewport] Gizmo switched to TRANSLATE" << std::endl; },
		[this]() { this->setGuizmoOperation(ImGuizmo::ROTATE); std::cout << "[OverlayViewport] Gizmo switched to ROTATE" << std::endl; },
		[this]() { this->setGuizmoOperation(ImGuizmo::SCALE); std::cout << "[OverlayViewport] Gizmo switched to SCALE" << std::endl; }
	);
	
	SIMILI::Input::KeyManager::getInstance().bindModeActions(
		[this](int mode) { this->switchModeByKey(mode); }
	);
}

OverlayViewport::~OverlayViewport() 
{
	if (selector_) 
	{
		delete selector_;
		selector_ = nullptr;
	}
	if (camera_control_) 
	{
		delete camera_control_;
		camera_control_ = nullptr;
	}
	if (raycast_performer_) 
	{
		delete raycast_performer_;
		raycast_performer_ = nullptr;
	}
	if (html_texture_renderer_) 
	{
		html_texture_renderer_ = nullptr; 
	}
	
	if (click_handler_) 
	{
		delete click_handler_;
		click_handler_ = nullptr;
	}
	
	if (normal_mode_) 
	{
		delete normal_mode_;
		normal_mode_ = nullptr;
	}
	if (vertice_mode_) 
	{
		delete vertice_mode_;
		vertice_mode_ = nullptr;
	}
	if (face_mode_) 
	{
		delete face_mode_;
		face_mode_ = nullptr;
	}
	if (edge_mode_) 
	{
		delete edge_mode_;
		edge_mode_ = nullptr;
	}
	
	destroy();
}

// ============================================================================
// WINDOW CREATION & MANAGEMENT
// ============================================================================

bool OverlayViewport::create(SDL_Window* parent, int x, int y, int width, int height) 
{
	parent_window_ = parent;
	width_ = width;
	height_ = height;
	
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	
	SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 1);
	
	sdl_window_ = SDL_CreateWindow(
		"OpenGL Overlay",
		width, height,
		SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN
	);
	
	if (!sdl_window_) 
	{
		std::cerr << "[OverlayViewport] Failed to create SDL window: " << SDL_GetError() << std::endl;
		return false;
	}
	
	SDL_SetWindowPosition(sdl_window_, x, y);
	
	if (!click_handler_) 
	{
		std::cerr << "[OverlayViewport] WARNING: Click handler not initialized!" << std::endl;
	}
	else 
	{
		std::cout << "[OverlayViewport] Click handler ready" << std::endl;
	}
	
	SDL_GLContext currentContext = SDL_GL_GetCurrentContext();
	if (currentContext) 
	{
		std::cout << "[OverlayViewport] Sharing current GL context" << std::endl;
	}
	
	initializeOpenGL(currentContext);		
	return true;
}

void OverlayViewport::destroy() 
{
	std::cout << "[OverlayViewport] Destroying overlay viewport..." << std::endl;
	
	shutdownImGui();
	
	if (gl_context_) {
		SDL_GL_DestroyContext(gl_context_);
		gl_context_ = nullptr;
	}
	
	if (sdl_window_) {
		SDL_DestroyWindow(sdl_window_);
		sdl_window_ = nullptr;
	}
}

void OverlayViewport::initializeOpenGL(SDL_GLContext shareContext) 
{
	gl_context_ = SDL_GL_CreateContext(sdl_window_);
	
	if (!gl_context_)
	{
		std::cerr << "[OverlayViewport] Failed to create OpenGL context: " << SDL_GetError() << std::endl;
		return;
	}
	
	if (SDL_GL_MakeCurrent(sdl_window_, gl_context_) != 0)
	{
		std::cerr << "[OverlayViewport] Failed to make context current: " << SDL_GetError() << std::endl;
		return;
	}
	
	static bool glad_loaded = false;

	if (!glad_loaded) 
	{
		if (!gladLoadGL()) 
		{
			std::cerr << "[OverlayViewport] Failed to load GLAD extensions" << std::endl;
			return;
		}
		glad_loaded = true;
	}
	
	std::cout << "[OverlayViewport] OpenGL context created - " 
			  << glGetString(GL_VERSION) << " - " << glGetString(GL_RENDERER) << std::endl;
	
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	std::cout << "[OverlayViewport] OpenGL state configured for 3D scene rendering" << std::endl;
	
	initializeImGui();
	
	html_texture_width_ = 350;
	html_texture_height_ = height_;
	html_texture_x_ = 10;
	html_texture_y_ = 10;
	
	html_texture_renderer_ = new HtmlTextureRenderer();
	html_texture_renderer_->initialize(html_texture_width_, html_texture_height_);
	html_texture_renderer_->setRenderRect(html_texture_x_, html_texture_y_, 
		html_texture_width_, html_texture_height_);
	html_texture_renderer_->createBrowser("file:///ui/Mode_UI.html", html_texture_width_, html_texture_height_);
}

void OverlayViewport::initializeImGui() 
{
	if (imgui_initialized_) 
	{
		return;
	}
	
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; 

	ImGui::StyleColorsDark();
	ImGui_ImplSDL3_InitForOpenGL(sdl_window_, gl_context_);
	ImGui_ImplOpenGL3_Init("#version 460");
	
	ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
	
	imgui_initialized_ = true;
}

void OverlayViewport::shutdownImGui() 
{
	if (!imgui_initialized_) {
		return;
	}
		
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
	
	imgui_initialized_ = false;
}

// ============================================================================
// OPENGL CONTEXT MANAGEMENT
// ============================================================================

void OverlayViewport::makeContextCurrent() {
	if (sdl_window_ && gl_context_) {
		int result = SDL_GL_MakeCurrent(sdl_window_, gl_context_);
		if (result != 0) {
			std::cerr << "[OverlayViewport::makeContextCurrent] FAILED! Error: " << SDL_GetError() << std::endl;
		}
	} else {
		std::cerr << "[OverlayViewport::makeContextCurrent] ERROR: Invalid SDL_Window or GL context!" << std::endl;
	}
}

void OverlayViewport::releaseContext() {
	SDL_GL_MakeCurrent(nullptr, nullptr);
}

// ============================================================================
// MESH OPENGL RESOURCE MANAGEMENT
// ============================================================================

void OverlayViewport::reinitializeMeshComponents(Mesh* mesh)
{
	if (!mesh) 
	{
		std::cerr << "[OverlayViewport] ERROR: Cannot reinitialize null mesh" << std::endl;
		return;
	}
	
	std::cout << "[OverlayViewport] Adding mesh '" << mesh->getName() << "' to pending finalization queue (will be finalized in render thread)..." << std::endl;
	
	std::lock_guard<std::mutex> lock(pending_meshes_mutex_);
	pending_meshes_to_finalize_.push_back(mesh);
	has_pending_meshes_.store(true, std::memory_order_release);

	
}

void OverlayViewport::update_Scene_Rendering()
{
	if (has_pending_meshes_.load(std::memory_order_acquire))
	{
		std::lock_guard<std::mutex> lock(pending_meshes_mutex_);
		if (!pending_meshes_to_finalize_.empty()) 		{
			
			for (Mesh* mesh : pending_meshes_to_finalize_) 
			{
				if (mesh) 
				{
					
					for (int i = 0; i < 10 && glGetError() != GL_NO_ERROR; ++i);
					
					mesh->finalize();
				
				}
			}
			
			pending_meshes_to_finalize_.clear();
			has_pending_meshes_.store(false, std::memory_order_release);
		}
	}
}

// ============================================================================
// RENDERING SYSTEM
// ============================================================================

void OverlayViewport::render() 
{
	if (!sdl_window_ || !rendering_enabled_) 
	{
		return;
	}
	
	update_Scene_Rendering();
	
	CefDoMessageLoopWork();
		
	if (imgui_initialized_) 
	{
		if (has_injected_inputs_)
		{
			ImGuiIO& io = ImGui::GetIO();
			io.MousePos = ImVec2(static_cast<float>(injected_mouse_x_), static_cast<float>(injected_mouse_y_));
			io.MouseDown[0] = injected_left_down_;
			io.MouseDown[1] = injected_right_down_;
			io.MouseDown[2] = injected_middle_down_;
			io.MouseWheel = injected_wheel_delta_;
			
			injected_wheel_delta_ = 0.0f;
		}
		
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
		
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImVec2(static_cast<float>(width_), static_cast<float>(height_)));
		ImGui::Begin("ViewportOverlay", nullptr, 
			ImGuiWindowFlags_NoTitleBar | 
			ImGuiWindowFlags_NoResize | 
			ImGuiWindowFlags_NoMove | 
			ImGuiWindowFlags_NoScrollbar | 
			ImGuiWindowFlags_NoScrollWithMouse | 
			ImGuiWindowFlags_NoCollapse | 
			ImGuiWindowFlags_NoBackground | 
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoFocusOnAppearing);
	}
	
	renderScene();
	
	ThreeDWorldInteractions();
	
	if (imgui_initialized_)
	{
		is_gizmo_active_ = ImGuizmo::IsUsing();
	}
	else
	{
		is_gizmo_active_ = false;
	}
	
	SIMILI::Input::KeyManager::getInstance().update();
	
	if (html_texture_renderer_) 
	{
		html_texture_renderer_->render();
	}
	
	if (imgui_initialized_) 
	{
		ImGui::End();
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}
	
	SDL_GL_SwapWindow(sdl_window_);
}

void OverlayViewport::updateViewportDimensions(int width, int height)
{
	if (width_ != width || height_ != height)
	{
		width_ = width;
		height_ = height;
		
		if (vk_scene_ && vk_scene_->getVKContext())
		{
			vk_scene_->getVKContext()->resize(width_, height_);
		}
	}
}

void OverlayViewport::renderScene() 
{
	static int render_debug_counter = 0;
	bool should_debug = false; // Disabled to reduce console spam
	
	// CRITICAL: Ensure our OpenGL context is current before rendering
	makeContextCurrent();
	
	if (vk_scene_) 
	{
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, width_, height_);
		
		if (should_debug) 
		{
			std::cout << "[OverlayViewport] Rendering 3D scene to " << width_ << "x" << height_ << " viewport" << std::endl;
		}
		
		// Enable depth testing for 3D rendering
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		
		vk_scene_->render(width_, height_);
		
		if (should_debug) 
		{
			// Check if anything was rendered
			unsigned char pixel[4];
			glReadPixels(width_/2, height_/2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
			std::cout << "[OverlayViewport] Center pixel after render: R=" << (int)pixel[0] << " G=" << (int)pixel[1] << " B=" << (int)pixel[2] << std::endl;
		}
	}
	else 
	{
		// Red background indicates missing 3D scene
		glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, width_, height_);
		
		if (should_debug) {
			std::cout << "[OverlayViewport] WARNING: No 3D scene to render!" << std::endl;
		}
	}
}

void OverlayViewport::setPosition(int x, int y, int width, int height) 
{
	if (sdl_window_) 
	{
		SDL_SetWindowPosition(sdl_window_, x, y);
		SDL_SetWindowSize(sdl_window_, width, height);
		
		width_ = width;
		height_ = height;
		
		glViewport(0, 0, width, height);
		
		if (vk_scene_ && vk_scene_->getActiveCamera() && ui_handler_ && ui_handler_->getFrameDatas())
		{
			SIMILI::Frontend::IFrameScreenData viewportData;
			if (ui_handler_->getFrameDatas()->getFrameData("viewport_docking", viewportData))
			{
				vk_scene_->getActiveCamera()->setResolution(width, height, viewportData.dpiScale);
			}
			else
			{
				vk_scene_->getActiveCamera()->setResolution(width, height);
			}
		}	
		
	}
}

void OverlayViewport::show(bool visible) 
{
	if (sdl_window_) 
	{
		if (visible)
		{
			SDL_ShowWindow(sdl_window_);
		}
		else
		{
			SDL_HideWindow(sdl_window_);
		}
	}
}

bool OverlayViewport::isVisible() const
{
	if (sdl_window_) {
		return (SDL_GetWindowFlags(sdl_window_) & SDL_WINDOW_HIDDEN) == 0;
	}
	return false;
}

void OverlayViewport::injectMouseInputs(int mouseX, int mouseY, bool leftDown, bool rightDown, bool middleDown, float wheelDelta)
{
	injected_mouse_x_ = mouseX;
	injected_mouse_y_ = mouseY;
	injected_left_down_ = leftDown;
	injected_right_down_ = rightDown;
	injected_middle_down_ = middleDown;
	injected_wheel_delta_ = wheelDelta;
	has_injected_inputs_ = true;
}

// ============================================================================
// SDL3 EVENT HANDLING
// ============================================================================

void OverlayViewport::handleEvents()
{
	Uint32 windowID = SDL_GetWindowID(sdl_window_);
	
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		bool isEventForThisWindow = false;
		
		if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST)
		{
			isEventForThisWindow = (event.window.windowID == windowID);
		}
		else if (event.type >= SDL_EVENT_KEY_DOWN && event.type <= SDL_EVENT_KEY_UP)
		{
			isEventForThisWindow = true;
		}
		else if (event.type >= SDL_EVENT_MOUSE_MOTION && event.type <= SDL_EVENT_MOUSE_WHEEL)
		{
			isEventForThisWindow = true;
		}
		
		if (isEventForThisWindow)
		{
			ImGui_ImplSDL3_ProcessEvent(&event);
		}
		
		if (event.type == SDL_EVENT_WINDOW_RESIZED && event.window.windowID == windowID)
		{
			int newWidth = event.window.data1;
			int newHeight = event.window.data2;
			
			if (newWidth > 0 && newHeight > 0)
			{
				width_ = newWidth;
				height_ = newHeight;
				
				if (gl_context_)
				{
					SDL_GLContext prevContext = SDL_GL_GetCurrentContext();
					SDL_Window* prevWindow = SDL_GL_GetCurrentWindow();
					SDL_GL_MakeCurrent(sdl_window_, gl_context_);
					glViewport(0, 0, newWidth, newHeight);
					if (prevContext && prevWindow)
					{
						SDL_GL_MakeCurrent(prevWindow, prevContext);
					}
				}
				
				if (html_texture_renderer_)
				{
					html_texture_renderer_->resize(newWidth, newHeight);
				}
			}
		}
		
		if (event.type == SDL_EVENT_KEY_DOWN)
		{
			SIMILI::Input::KeyManager::getInstance().handleKeyDown(static_cast<int>(event.key.key), 0);
		}
		else if (event.type == SDL_EVENT_KEY_UP)
		{
			SIMILI::Input::KeyManager::getInstance().handleKeyUp(static_cast<int>(event.key.key));
		}
	}
}

// ============================================================================
// RAYCAST & OBJECT SELECTION
// ============================================================================

void OverlayViewport::performRaycast(int mouseX, int mouseY) 
{
	if (!selector_ || !vk_scene_)
	{
		return;
	}
	
	Camera* camera = vk_scene_->getActiveCamera();
	if (!camera)
	{
		return;
	}
	
	float aspect = (height_ > 0) ? static_cast<float>(width_) / static_cast<float>(height_) : 1.0f;
	glm::mat4 view = camera->getViewMatrix();
	glm::mat4 projection = camera->getProjectionMatrix(aspect);
	
	auto& objects = vk_scene_->getObjectsRef();
	std::vector<ThreeDObject*> objectsVector(objects.begin(), objects.end());
	
	selector_->pickUpMesh(mouseX, mouseY, width_, height_, view, projection, objectsVector);
}

void OverlayViewport::setMultipleSelectedObjects(const std::list<ThreeDObject*>& objects) 
{ 
	multiple_selected_objects_ = objects;
	
	if (ui_handler_) 
	{
		ui_handler_->notifySceneChanged();
	}
}

// ============================================================================
// 3D MODELING MODE MANAGEMENT
// ============================================================================

void OverlayViewport::setModelingMode(ThreeDMode* mode)
{
	if (mode)
	{
		current_mode_ = mode;
	}
}

void OverlayViewport::switchModeByKey(int keyNumber)
{
	switch (keyNumber)
	{
		case 1:
			setModelingMode(normal_mode_);
			break;
		case 2:
			setModelingMode(edge_mode_);
			break;
		case 3:
			setModelingMode(vertice_mode_);
			break;
		case 4:
			setModelingMode(face_mode_);
			break;
		default:
			break;
	}
}

// ============================================================================
// 3D OBJECT MANIPULATION & GIZMO
// ============================================================================

void OverlayViewport::ThreeDWorldInteractions()
{
	if (!vk_scene_)
		return;
	
	Camera* camera = vk_scene_->getActiveCamera();
	if (!camera)
		return;
	
	float aspect = (height_ > 0) ? static_cast<float>(width_) / static_cast<float>(height_) : 1.0f;
	
	glm::mat4 view = camera->getViewMatrix();
	glm::mat4 projection = camera->getProjectionMatrix(aspect);
	ImVec2 oglChildPos(0.0f, 0.0f);
	ImVec2 oglChildSize(static_cast<float>(width_), static_cast<float>(height_));
	
	if (current_mode_ == normal_mode_)
	{
		MeshTransform::manipulateMesh(
			vk_scene_,
			multiple_selected_objects_,
			oglChildPos,
			oglChildSize,
			was_using_gizmo_last_frame_,
			view,
			projection
		);
	}
	else if (current_mode_ == vertice_mode_)
	{
		VerticeTransform::manipulateVertices(
			vk_scene_,
			multiple_selected_vertices_,
			oglChildPos,
			oglChildSize,
			was_using_gizmo_last_frame_,
			view,
			projection
		);
	}
	else if (current_mode_ == face_mode_)
	{
		FaceTransform::manipulateFaces(
			vk_scene_,
			multiple_selected_faces_,
			oglChildPos,
			oglChildSize,
			was_using_gizmo_last_frame_,
			true,
			view,
			projection
		);
	}
	else if (current_mode_ == edge_mode_)
	{
		EdgeTransform::manipulateEdges(
			vk_scene_,
			multiple_selected_edges_,
			oglChildPos,
			oglChildSize,
			was_using_gizmo_last_frame_,
			nullptr, 
			view,
			projection
		);
	}	
}


void OverlayViewport::MoveCameraLaterally(int deltaX, int deltaY)
{
	if (!vk_scene_) return;
	Camera* cam = vk_scene_->getActiveCamera();
	if (!cam || !cam->isSoftwareCamera()) return;
	
	const float sensitivity = 1.0f;
	
	float dx = static_cast<float>(deltaX) * sensitivity;
	float dy = static_cast<float>(deltaY) * sensitivity;
	
	cam->lateralMovement(dx, dy);
	// No need to manually invalidate with SDL3 - continuous render loop handles updates
}

void OverlayViewport::ProcessWheelInput(int wheelDirection)
{
	std::cout << "[OVERLAY_VIEWPORT] Wheel direction received: " << wheelDirection << std::endl;
}

void OverlayViewport::ProcessZoom(int wheelDirection)
{
	if (!camera_control_) return;
	camera_control_->onZoom(wheelDirection);
}

void OverlayViewport::ProcessMouseMovementWhileLeftClicking(int deltaX, int deltaY)
{
	std::cout << "[OVERLAY_VIEWPORT] Mouse movement while left clicking - DeltaX: " << deltaX << ", DeltaY: " << deltaY << std::endl;
}

void OverlayViewport::ProcessCameraOrbiting(int deltaX, int deltaY)
{
	if (!vk_scene_) return;
	Camera* cam = vk_scene_->getActiveCamera();
	if (!cam || !cam->isSoftwareCamera()) return;
	
	cam->prepareOrbit();
	cam->orbitAroundTarget(static_cast<float>(deltaX), static_cast<float>(deltaY));
	// No need to manually invalidate with SDL3 - continuous render loop handles updates
}

// ---------- Raycast process ------------- //

void OverlayViewport::shootRaycastFromUIHandler(int mouseX, int mouseY)
{
	if (!vk_scene_ || !selector_ || !sdl_window_)
	{
		return;
	}
	
	Camera* cam = vk_scene_->getActiveCamera();
	if (!cam)
	{
		return;
	}
	
		int wx, wy;
		SDL_GetWindowPosition(sdl_window_, &wx, &wy);
		
		int localMouseX = mouseX - wx;
		int localMouseY = mouseY - wy;
			
		float dpiScale = cam->getDpiScale();
		int resolutionWidth = cam->getResolutionWidth();
		int resolutionHeight = cam->getResolutionHeight();
		
		glm::mat4 view = cam->getViewMatrix();
		float aspectRatio = static_cast<float>(resolutionWidth) / static_cast<float>(resolutionHeight);
		glm::mat4 projection = cam->getProjectionMatrix(aspectRatio);
		
		auto& objects = vk_scene_->getObjectsRef();
		std::vector<ThreeDObject*> objectsVector(objects.begin(), objects.end());
		
		if (current_mode_ == normal_mode_)
		{
			selector_->pickUpMesh(localMouseX, localMouseY, resolutionWidth, resolutionHeight, view, projection, objectsVector);
			
			if (vk_scene_ && selector_)
			{
				auto& objects = vk_scene_->getObjectsRef();
				ThreeDObject* clickedObject = selector_->getSelectedObject();
				
				for (auto* obj : objects)
				{
					if (obj) obj->setSelected(false);
				}
				
				if (clickedObject && clickedObject->isSelectable())
				{
					clickedObject->setSelected(true);
				}
				
				std::list<ThreeDObject*> selectedList;
				for (auto* obj : objects)
				{
					if (obj && obj->getSelected())
					{
						selectedList.push_back(obj);
					}
				}
				
				setMultipleSelectedObjects(selectedList);

			}
		}
		else if (current_mode_ == face_mode_)
		{
			Face* clickedFace = selector_->pickupFace(localMouseX, localMouseY, resolutionWidth, resolutionHeight, view, projection, objectsVector, true);
			
			if (clickedFace)
			{
				multiple_selected_faces_.clear();
				multiple_selected_faces_.push_back(clickedFace);
			}
			else
			{
				multiple_selected_faces_.clear();
			}
			

		}
		else if (current_mode_ == vertice_mode_)
		{
			Vertice* clickedVertice = selector_->pickUpVertice(localMouseX, localMouseY, resolutionWidth, resolutionHeight, view, projection, objectsVector, true);
			
			if (clickedVertice)
			{
				multiple_selected_vertices_.clear();
				multiple_selected_vertices_.push_back(clickedVertice);
			}
			else
			{
				multiple_selected_vertices_.clear();
			}
			
		}
		else if (current_mode_ == edge_mode_)
		{
			Edge* clickedEdge = selector_->pickupEdge(localMouseX, localMouseY, resolutionWidth, resolutionHeight, view, projection, objectsVector, true);
			
			if (clickedEdge)
			{
				multiple_selected_edges_.clear();
				multiple_selected_edges_.push_back(clickedEdge);
			}
			else
			{
				multiple_selected_edges_.clear();
			}
		
		}
}