#include <iostream>
#include <glad/glad.h>

// ImGui MUST be included BEFORE ImGuizmo
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>
#include <ImGuizmo.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>

#include "overlay_viewport.hpp"
#include "CameraControl/CameraControl.hpp"
#include "Raycasting/RaycastPerform.hpp"
#include "HTMLTextureRenderer/TextureRendererTest.hpp"
#include "HTMLTextureRenderer/HtmlTextureRenderer.hpp"
#include "HTMLTextureRenderer/SlotTexture.hpp"
#include "ClickHandling/OverlayClickHandler.hpp"
#include "ContextualMenuLogic/ContextualMenuTextureTest.hpp"

#include "../../Engine/ThreeDScene.hpp"
#include "../../Engine/OpenGLContext.hpp"
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
#include "../../../UI/ThreeDModes/ThreeDMode.hpp"
#include "../../../UI/ThreeDModes/Normal_Mode.hpp"
#include "../../../UI/ThreeDModes/Vertice_Mode.hpp"
#include "../../../UI/ThreeDModes/Face_Mode.hpp"
#include "../../../UI/ThreeDModes/Edge_Mode.hpp"

#include "../../Engine/ThreeDInteractions/MeshTransform.hpp"
#include "../../Engine/ThreeDInteractions/VerticeTransform.hpp"
#include "../../Engine/ThreeDInteractions/FaceTransform.hpp"
#include "../../Engine/ThreeDInteractions/EdgeTransform.hpp"
#include "Keymanagement/KeyManager.hpp"
#include "../ui_handler.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#include <commctrl.h> 
#include <windowsx.h> 
#pragma comment(lib, "comctl32.lib")

#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif

typedef HGLRC (WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC hDC, HGLRC hShareContext, const int *attribList);

namespace 
{
	const wchar_t* kOverlayClassName = L"SIMILI_OpenGL_Overlay";
}

// ============================================================================
// PARENT WINDOW MESSAGE INTERCEPTION
// ============================================================================

LRESULT CALLBACK OverlayViewport::ParentSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, 
UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	OverlayViewport* overlay = reinterpret_cast<OverlayViewport*>(dwRefData);
	
	// Handle window resize events to ensure proper coordinate tracking
	if (msg == WM_SIZE || msg == WM_WINDOWPOSCHANGED) {
		if (overlay && overlay->hwnd_) {
			// Force overlay position/size update
			overlay->ensureProperZOrder();
			
			// Debug: Show parent window size changes
			if (msg == WM_SIZE) {
				int parentWidth = LOWORD(lParam);
				int parentHeight = HIWORD(lParam);
				std::cout << "[ParentSubclassProc] Parent window resized to: " << parentWidth << "x" << parentHeight << std::endl;
				
				// Force overlay to update its internal size tracking
				RECT overlayRect;
				GetWindowRect(overlay->hwnd_, &overlayRect);
				int overlayWidth = overlayRect.right - overlayRect.left;
				int overlayHeight = overlayRect.bottom - overlayRect.top;
				std::cout << "[ParentSubclassProc] Overlay size: " << overlayWidth << "x" << overlayHeight << std::endl;
			}
		}
	}
	
	// Forward mouse and keyboard messages to the OverlayViewport
	if (overlay && overlay->hwnd_) {
		switch (msg) {
			case WM_MOUSEWHEEL:
			case WM_MBUTTONDOWN:
			case WM_MBUTTONUP:
			case WM_MOUSEMOVE:
			{
				// Get the mouse position in parent window coordinates
				POINT ptParent = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
				
				// Get CURRENT window rectangles (critical after resize events)
				RECT overlayRect, parentRect;
				GetWindowRect(overlay->hwnd_, &overlayRect);
				GetClientRect(hwnd, &parentRect);
				
				// Get CURRENT overlay dimensions (not cached values that may be stale after resize)
				int currentOverlayWidth = overlayRect.right - overlayRect.left;
				int currentOverlayHeight = overlayRect.bottom - overlayRect.top;
				
				// Convert overlay position to parent client coordinates
				POINT overlayTopLeft = { overlayRect.left, overlayRect.top };
				ScreenToClient(hwnd, &overlayTopLeft);
				
				// Calculate relative position within overlay
				int relativeX = ptParent.x - overlayTopLeft.x;
				int relativeY = ptParent.y - overlayTopLeft.y;
				
				// Debug for mouse wheel specifically - show CURRENT sizes
				if (msg == WM_MOUSEWHEEL) {
					std::cout << "[ParentSubclassProc] WHEEL - Parent: (" << ptParent.x << ", " << ptParent.y 
					          << ") -> Overlay: (" << relativeX << ", " << relativeY 
					          << ") | OverlayPos: (" << overlayTopLeft.x << ", " << overlayTopLeft.y << ")"
					          << " | CurrentOverlaySize: " << currentOverlayWidth << "x" << currentOverlayHeight
					          << " | CachedSize: " << overlay->width_ << "x" << overlay->height_ << std::endl;
				}
				
				// Forward to overlay with adjusted coordinates
				LPARAM newLParam = MAKELPARAM(relativeX, relativeY);
				SendMessage(overlay->hwnd_, msg, wParam, newLParam);
				break;
			}
		}
	}
	
	// Call the original window procedure
	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// LIFECYCLE MANAGEMENT
// ============================================================================


OverlayViewport::OverlayViewport() : hwnd_(nullptr) 
	, parent_(nullptr)
	, hdc_(nullptr)
	, gl_context_(nullptr)
	, width_(800)
	, height_(600)
	, three_d_scene_(nullptr)
	, rendering_enabled_(true)
	, imgui_initialized_(false)
	, selector_(nullptr)
	, current_guizmo_operation_(ImGuizmo::TRANSLATE)
	, current_guizmo_mode_(ImGuizmo::LOCAL)
	, texture_renderer_test_(nullptr)
	, html_texture_renderer_(nullptr)
	, normal_mode_(nullptr)
	, vertice_mode_(nullptr)
	, face_mode_(nullptr)
	, edge_mode_(nullptr)
	, current_mode_(nullptr)
	, was_using_gizmo_last_frame_(false)
	, contextual_menu_texture_test_(nullptr)
	, contextual_menu_texture_renderer_(nullptr)
	, contextual_menu_html_renderer_(nullptr)
	, slot_texture_(nullptr)
{
	selector_ = new ThreeDObjectSelector();
	camera_control_ = new CameraControl(this);
	raycast_performer_ = new RaycastPerform(this, selector_);
	texture_renderer_test_ = new TextureRendererTest();
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
	if (texture_renderer_test_) 
	{
		delete texture_renderer_test_;
		texture_renderer_test_ = nullptr;
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
	
	if (contextual_menu_texture_test_) 
	{
		delete contextual_menu_texture_test_;
		contextual_menu_texture_test_ = nullptr;
	}
	
	if (contextual_menu_texture_renderer_) 
	{
		delete contextual_menu_texture_renderer_;
		contextual_menu_texture_renderer_ = nullptr;
	}
	
	if (contextual_menu_html_renderer_) 
	{
		contextual_menu_html_renderer_ = nullptr;
	}
	
	if (slot_texture_) 
	{
		delete slot_texture_;
		slot_texture_ = nullptr;
	}
	
	destroy();
}

// ============================================================================
// WINDOW CREATION & MANAGEMENT
// ============================================================================

bool OverlayViewport::create(HWND parent, int x, int y, int width, int height) 
{
	parent_ = parent;
	width_ = width;
	height_ = height;
	
	// Register window class
	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = nullptr;
	wc.lpszClassName = kOverlayClassName;
	
	static bool class_registered = false;

	if (!class_registered) 
	{
		if (!RegisterClassExW(&wc)) 
		{
			DWORD err = GetLastError();
			return false;
		}
		class_registered = true;
	}
	
	hwnd_ = CreateWindowExW(
		0,  // No extended styles - standard child window
		kOverlayClassName,
		L"OpenGL Overlay",
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,  // CHILD window that clips siblings
		x, y, width, height,
		parent,
		nullptr,
		GetModuleHandle(nullptr),
		this
	);
	
	if (!hwnd_) 
	{
		DWORD error = GetLastError();
		std::cerr << "[OverlayViewport] Failed to create window, error: " << error << std::endl;
		return false;
	}
	
	// Set as top within parent's child windows (NOT TOPMOST to avoid always-on-top behavior)
	// Using HWND_TOP keeps it above siblings without making it globally topmost
	SetWindowPos(hwnd_, HWND_TOP, 0, 0, 0, 0, 
				 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	
	// Ensure we don't have TOPMOST extended style
	DWORD exStyle = GetWindowLongW(hwnd_, GWL_EXSTYLE);
	if (exStyle & WS_EX_TOPMOST) {
		SetWindowLongW(hwnd_, GWL_EXSTYLE, exStyle & ~WS_EX_TOPMOST);
		std::cout << "[OverlayViewport] Removed WS_EX_TOPMOST flag" << std::endl;
	}
	
	std::cout << "[OverlayViewport] Created as CHILD window with Z-order control (NOT always-on-top)" << std::endl;
	
	// Install a message hook on the parent window to forward mouse messages to us
	// This ensures we receive mouse messages even when SlotTexture is covering parts of the viewport
	SetWindowSubclass(parent_, ParentSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
	std::cout << "[OverlayViewport] Installed message hook on parent window" << std::endl;
	
	if (!click_handler_) 
	{
		std::cerr << "[OverlayViewport] WARNING: Click handler not initialized!" << std::endl;
	}
	else 
	{
		std::cout << "[OverlayViewport] Click handler ready" << std::endl;
	}
	
	// Get GLFW context from current thread to share resources
	HGLRC glfwContext = wglGetCurrentContext();
	if (glfwContext) {
		std::cout << "[OverlayViewport] Sharing GLFW context: " << glfwContext << std::endl;
	}
	
	initializeOpenGL(glfwContext);		
	return true;
}void OverlayViewport::destroy() 
{
	std::cout << "[OverlayViewport] Destroying overlay viewport..." << std::endl;
	
	// Remove the subclass from parent window
	if (parent_) {
		RemoveWindowSubclass(parent_, ParentSubclassProc, 1);
		std::cout << "[OverlayViewport] Removed message hook from parent window" << std::endl;
	}
	
	shutdownImGui();
	
	if (gl_context_) {
		wglMakeCurrent(nullptr, nullptr);
		wglDeleteContext(gl_context_);
		gl_context_ = nullptr;
	}
	
	if (hdc_) 
	{
		ReleaseDC(hwnd_, hdc_);
		hdc_ = nullptr;
	}
	
	if (hwnd_) {
		DestroyWindow(hwnd_);
		hwnd_ = nullptr;
	}
}

void OverlayViewport::initializeOpenGL(HGLRC shareContext) 
{
	hdc_ = GetDC(hwnd_);
	
	PIXELFORMATDESCRIPTOR pfd = {};
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE;
	
	int pixelFormat = ChoosePixelFormat(hdc_, &pfd);
	if (!pixelFormat) {
		std::cerr << "[OverlayViewport] Failed to choose pixel format" << std::endl;
		return;
	}
	
	if (!SetPixelFormat(hdc_, pixelFormat, &pfd)) {
		std::cerr << "[OverlayViewport] Failed to set pixel format" << std::endl;
		return;
	}
	
	// Create OpenGL context SHARED with GLFW context
	HGLRC tempContext = wglCreateContext(hdc_);
	if (!tempContext) {
		std::cerr << "[OverlayViewport] Failed to create temporary OpenGL context" << std::endl;
		return;
	}
	
	wglMakeCurrent(hdc_, tempContext);
	
	// Load wglCreateContextAttribsARB extension
	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = 
		(PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
	
	if (wglCreateContextAttribsARB) {
		int attribs[] = {
			WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
			WGL_CONTEXT_MINOR_VERSION_ARB, 3,
			WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
			0
		};
		
		std::cout << "[OverlayViewport] Attempting to create shared context with GLFW context: " << shareContext << std::endl;
		
		// CRITICAL: Share with GLFW context to access VAO/VBO/Shaders!
		gl_context_ = wglCreateContextAttribsARB(hdc_, shareContext, attribs);
		if (gl_context_) {
			wglMakeCurrent(nullptr, nullptr);
			wglDeleteContext(tempContext);
			wglMakeCurrent(hdc_, gl_context_);
			
			if (shareContext) {
				std::cout << "[OverlayViewport] ✓ OpenGL 3.3 Core context created SHARED with GLFW context " << shareContext << std::endl;
			} else {
				std::cout << "[OverlayViewport] ✗ OpenGL 3.3 Core context created WITHOUT sharing (shareContext was NULL)" << std::endl;
			}
		} else {
			DWORD err = GetLastError();
			std::cerr << "[OverlayViewport] Failed to create shared OpenGL 3.3 context (error: " << err << "), using compatibility context" << std::endl;
			gl_context_ = tempContext;
		}
	} else {
		std::cout << "[OverlayViewport] wglCreateContextAttribsARB not available, using compatibility context" << std::endl;
		gl_context_ = tempContext;
	}
	
	static bool glad_loaded = false;
	if (!glad_loaded) {
		if (!gladLoadGL()) 
		{
			std::cerr << "[OverlayViewport] Failed to load GLAD extensions" << std::endl;
			return;
		}
		glad_loaded = true;
	}
	
	std::cout << "[OverlayViewport] Shared OpenGL context created - " 
			  << glGetString(GL_VERSION) << " - " << glGetString(GL_RENDERER) << std::endl;
	
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	std::cout << "[OverlayViewport] OpenGL state configured for 3D scene rendering" << std::endl;
	
	// Initialize ImGui after OpenGL context is ready
	initializeImGui();
	
	// Initialize texture renderer test
	if (texture_renderer_test_) {
		texture_renderer_test_->initialize(width_, height_);
		
		html_texture_width_ = 350;
		html_texture_height_ = height_;
		html_texture_x_ = 10;
		html_texture_y_ = 10;
		
		// Configure render rectangle for the texture
		texture_renderer_test_->setRenderRect(html_texture_x_, html_texture_y_, 
		html_texture_width_, html_texture_height_);
		
		html_texture_renderer_ = new HtmlTextureRenderer(texture_renderer_test_);
		html_texture_renderer_->createBrowser("file:///ui/Mode_UI.html", html_texture_width_, html_texture_height_);
		
		// CRITICAL: Pass viewport HWND so renderer can trigger immediate redraws
		html_texture_renderer_->setViewportWindow(hwnd_);
	
	}
	
	contextual_menu_width_ = 300;
	contextual_menu_height_ = 200;
	contextual_menu_x_ = 100;
	contextual_menu_y_ = 100;
	
	contextual_menu_texture_test_ = new ContextualMenuTextureTest();
	contextual_menu_texture_test_->initialize(contextual_menu_width_, contextual_menu_height_);
	
	contextual_menu_texture_renderer_ = new TextureRendererTest();
	contextual_menu_texture_renderer_->initialize(width_, height_);
	
	contextual_menu_texture_renderer_->setRenderRect(contextual_menu_x_, contextual_menu_y_, 
	contextual_menu_width_, contextual_menu_height_);
	
	contextual_menu_html_renderer_ = new HtmlTextureRenderer(contextual_menu_texture_renderer_);
	
	contextual_menu_html_renderer_->setOnBrowserCreatedCallback([this](CefRefPtr<CefBrowser> browser) 
	{
		if (contextual_menu_texture_test_) 
		{
			contextual_menu_texture_test_->setBrowser(browser);
			std::cout << "[OverlayViewport] CEF browser connected to ContextualMenuTextureTest (browser valid: " 
					  << (browser != nullptr) << ", host valid: " << (browser && browser->GetHost() != nullptr) << ")" << std::endl;
		} 
	});
	
	contextual_menu_html_renderer_->createBrowser("file:///ui/Contextual_Menu.html", 
	contextual_menu_width_, contextual_menu_height_);

	contextual_menu_html_renderer_->setViewportWindow(hwnd_);
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
	ImGui_ImplWin32_Init(hwnd_);
	ImGui_ImplOpenGL3_Init("#version 460");
	
	// Initialize ImGuizmo
	ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
	
	imgui_initialized_ = true;
}

void OverlayViewport::shutdownImGui() 
{
	if (!imgui_initialized_) {
		return;
	}
		
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	
	imgui_initialized_ = false;
}

// ============================================================================
// OPENGL CONTEXT MANAGEMENT
// ============================================================================

void OverlayViewport::makeContextCurrent() {
	if (hdc_ && gl_context_) {
		BOOL result = wglMakeCurrent(hdc_, gl_context_);
		if (!result) {
			DWORD error = GetLastError();
			std::cerr << "[OverlayViewport::makeContextCurrent] FAILED! Error code: " << error << std::endl;
			std::cerr << "[OverlayViewport::makeContextCurrent] HDC: " << hdc_ << ", Context: " << gl_context_ << std::endl;
		}
		// Success log removed to reduce console spam
	} else {
		std::cerr << "[OverlayViewport::makeContextCurrent] ERROR: Invalid HDC or GL context!" << std::endl;
		std::cerr << "[OverlayViewport::makeContextCurrent] HDC: " << hdc_ << ", Context: " << gl_context_ << std::endl;
	}
}

void OverlayViewport::releaseContext() {
	wglMakeCurrent(nullptr, nullptr);
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
	if (!hwnd_ || !rendering_enabled_) 
	{
		return;
	}
	
	
	update_Scene_Rendering();
	
	CefDoMessageLoopWork();
		
	if (imgui_initialized_) 
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplWin32_NewFrame();
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
	
	SIMILI::Input::KeyManager::getInstance().update();
	
	if (texture_renderer_test_) {
		texture_renderer_test_->render();
	}
	
	// Ensure SlotTexture remains on top after 3D scene rendering (less frequent)
	static int z_order_check_counter = 0;
	if (slot_texture_ && (z_order_check_counter % 60 == 0)) { // Check every 60 frames
		slot_texture_->ensureProperZOrder();
	}
	z_order_check_counter++;
	
	if (contextual_menu_visible_ && contextual_menu_texture_renderer_) 
	{
		contextual_menu_texture_renderer_->render();
	}
	
	// Render SlotTexture (Layer 2 - Above everything else)
	if (slot_texture_) {
		slot_texture_->render();
	}
	
	if (imgui_initialized_) 
	{
		ImGui::End();
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}
	
	SwapBuffers(hdc_);
}

void OverlayViewport::renderScene() 
{
	static int render_debug_counter = 0;
	bool should_debug = false; // Disabled to reduce console spam
	
	// CRITICAL: Ensure our OpenGL context is current before rendering
	makeContextCurrent();
	
	if (three_d_scene_) 
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
		
		three_d_scene_->render(width_, height_);
		
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

// ============================================================================
// WINDOWS MESSAGE HANDLING
// ============================================================================

void OverlayViewport::setPosition(int x, int y, int width, int height) 
{
	if (hwnd_ && parent_) 
	{
		// Use client coordinates since we're a CHILD window
		// HWND_TOP keeps it above siblings without global topmost
		SetWindowPos(hwnd_, HWND_TOP, x, y, width, height, 
					 SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOZORDER);
		
		width_ = width;
		height_ = height;
		
		glViewport(0, 0, width, height);
		
		if (texture_renderer_test_) 
		{
			texture_renderer_test_->resize(width, height);
		}
		
		if (contextual_menu_texture_renderer_) 
		{
			contextual_menu_texture_renderer_->resize(width, height);
		}
		
	}
}

void OverlayViewport::show(bool visible) 
{
	if (hwnd_) 
	{
		ShowWindow(hwnd_, visible ? SW_SHOW : SW_HIDE);
		
		// Re-assert Z-order after show/hide to maintain proper layering
		if (visible) 
		{
			ensureProperZOrder();
		}
	}
}

bool OverlayViewport::isVisible() const
{
	if (hwnd_) {
		return IsWindowVisible(hwnd_) != 0;
	}
	return false;
}

void OverlayViewport::ensureProperZOrder()
{
	if (!hwnd_ || !parent_) return;
	
	// Layer-based Z-order system:
	// Layer 0: CEF browser (bottom)
	// Layer 1: OpenGL overlay (on top of CEF)
	// Higher layers = closer to user
	
	if (z_order_layer_ == 0) 
	{
		// Layer 0: Place at bottom of Z-order
		SetWindowPos(hwnd_, HWND_BOTTOM, 0, 0, 0, 0, 
					 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	} 
	else 
	{
		// Layer 1: Position after parent (CEF) but avoid constant repositioning
		// Only reposition if we're not already in the right place
		static bool positioned_once = false;
		if (!positioned_once) {
			SetWindowPos(hwnd_, parent_, 0, 0, 0, 0, 
						 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
			positioned_once = true;
			std::cout << "[OverlayViewport] Positioned after parent (Layer 1)" << std::endl;
		}
	}
	
	// Ensure TOPMOST flag is never set (prevents always-on-top behavior)
	DWORD exStyle = GetWindowLongW(hwnd_, GWL_EXSTYLE);
	if (exStyle & WS_EX_TOPMOST) 
	{
		SetWindowLongW(hwnd_, GWL_EXSTYLE, exStyle & ~WS_EX_TOPMOST);
		SetWindowPos(hwnd_, HWND_NOTOPMOST, 0, 0, 0, 0, 
					 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		std::cout << "[OverlayViewport] Removed TOPMOST flag (layer=" << z_order_layer_ << ")" << std::endl;
	}
}

// ============================================================================
// WINDOWS MESSAGE HANDLING
// ============================================================================

LRESULT CALLBACK OverlayViewport::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) 
{
	OverlayViewport* overlay = nullptr;
	
	if (msg == WM_CREATE) 
	{
		CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
		overlay = static_cast<OverlayViewport*>(cs->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(overlay));
	} 
	else 
	{
		overlay = reinterpret_cast<OverlayViewport*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	}
	
	ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
	
	bool shouldProcessEvent = true;
	if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) 
	{
		if (ImGuizmo::IsUsing()) 
		{
			shouldProcessEvent = false;
		}
	}
	
	if (overlay) 
	{
		switch (msg) 
		{
			case WM_PAINT: 
			{
				PAINTSTRUCT ps;
				BeginPaint(hwnd, &ps);
				overlay->render();
				EndPaint(hwnd, &ps);
				return 0;
			}
			
			case WM_SIZE:
			{
				// Handle overlay viewport size changes
				int newWidth = LOWORD(lParam);
				int newHeight = HIWORD(lParam);
				
				if (newWidth > 0 && newHeight > 0) {
					// Update internal dimensions IMMEDIATELY
					overlay->width_ = newWidth;
					overlay->height_ = newHeight;
					
					// Update OpenGL viewport
					if (overlay->gl_context_) {
						HGLRC prevContext = wglGetCurrentContext();
						HDC prevDC = wglGetCurrentDC();
						wglMakeCurrent(overlay->hdc_, overlay->gl_context_);
						glViewport(0, 0, newWidth, newHeight);
						if (prevContext && prevDC) {
							wglMakeCurrent(prevDC, prevContext);
						}
					}
					
					std::cout << "[OverlayViewport] WM_SIZE - New size: " << newWidth << "x" << newHeight 
					          << " (internal dimensions updated)" << std::endl;
					
					// Update texture renderers
					if (overlay->texture_renderer_test_) {
						overlay->texture_renderer_test_->resize(newWidth, newHeight);
					}
					if (overlay->contextual_menu_texture_renderer_) {
						overlay->contextual_menu_texture_renderer_->resize(newWidth, newHeight);
					}
				}
				return 0;
			}
			
			case WM_ERASEBKGND:
				return 1;
			
			case WM_WINDOWPOSCHANGING:
			{
				// Intercept window position changes to prevent unwanted Z-order modifications
				WINDOWPOS* wp = reinterpret_cast<WINDOWPOS*>(lParam);
				
				// If someone tries to set us as TOPMOST, prevent it
				if (wp->hwndInsertAfter == HWND_TOPMOST) 
				{
					wp->hwndInsertAfter = HWND_TOP;
					std::cout << "[OverlayViewport] Prevented TOPMOST positioning" << std::endl;
				}
				break;
			}
			
			case WM_ACTIVATE:
			{
				// Maintain Z-order after activation events
				if (LOWORD(wParam) != WA_INACTIVE) 
				{
					overlay->ensureProperZOrder();
				}
				break;
			}
			
			case WM_NCHITTEST:
			{
				return HTTRANSPARENT;
			}
			
			// ------ MODE SWITCHING -----
			
			case WM_KEYDOWN:
			case WM_KEYUP:
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_CHAR:
			{
				// Forward keyboard events to ImGui FIRST
				ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
				
				if (msg == WM_KEYDOWN)
				{
					SIMILI::Input::KeyManager::getInstance().handleKeyDown(static_cast<int>(wParam), lParam);
				}
				else if (msg == WM_KEYUP)
				{
					SIMILI::Input::KeyManager::getInstance().handleKeyUp(static_cast<int>(wParam));
				}
				return 0;
			}
		}
	}
	
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ============================================================================
// RAYCAST & OBJECT SELECTION
// ============================================================================

void OverlayViewport::performRaycast(int mouseX, int mouseY) 
{
	if (raycast_performer_) {
		raycast_performer_->performRaycast(mouseX, mouseY);
	}
}

void OverlayViewport::setMultipleSelectedObjects(const std::list<ThreeDObject*>& objects) 
{ 
	multiple_selected_objects_ = objects;
	
	if (ui_handler_) {
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
	if (!three_d_scene_)
		return;
	
	Camera* camera = three_d_scene_->getActiveCamera();
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
			three_d_scene_,
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
			three_d_scene_,
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
			three_d_scene_,
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
			three_d_scene_,
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


// ============================================================================
// CONTEXTUAL MENU MANAGEMENT
// ============================================================================

void OverlayViewport::setContextualMenuPosition(int x, int y)
{
	contextual_menu_x_ = x;
	contextual_menu_y_ = y;
	
	if (contextual_menu_texture_renderer_) 
	{
		contextual_menu_texture_renderer_->setRenderRect(x, y, 
			contextual_menu_width_, contextual_menu_height_);
	}
}

// ============================================================================
// SLOT TEXTURE MANAGEMENT (Layer 2 - Above Everything)
// ============================================================================

void OverlayViewport::createSlotTexture(int x, int y, int width, int height)
{
	if (!parent_) 
	{
		std::cerr << "[OverlayViewport] Cannot create SlotTexture - no parent window" << std::endl;
		return;
	}
	
	if (slot_texture_) {
		delete slot_texture_;
		slot_texture_ = nullptr;
	}
	
	slot_texture_ = new SlotTexture();
	

	if (slot_texture_->create(parent_, x, y, width, height, 2)) 
	{
		// Configure SlotTexture to display CEF HTML content
		slot_texture_->loadHTML("file:///ui/hello_cef.html");
		slot_texture_->setUseHTMLTexture(true);
		
		// Enable rendering
		slot_texture_->enableRendering(true);
				
		// Show the window
		slot_texture_->show(true);
		
		slot_texture_->ensureProperZOrder();
		
		// Force initial render
		slot_texture_->render();
		InvalidateRect(slot_texture_->getHandle(), nullptr, TRUE);
		UpdateWindow(slot_texture_->getHandle());
		
	} 
	else
	 {
		std::cerr << "[OverlayViewport] Failed to create SlotTexture" << std::endl;
		delete slot_texture_;
		slot_texture_ = nullptr;
	}
}

void OverlayViewport::destroySlotTexture()
{
	if (slot_texture_) {
		delete slot_texture_;
		slot_texture_ = nullptr;
	}
}

void OverlayViewport::showSlotTexture(bool visible)
{
	if (slot_texture_) {
		slot_texture_->show(visible);
		
		if (visible) 
		{
			InvalidateRect(slot_texture_->getHandle(), nullptr, FALSE);
		}
	}
}