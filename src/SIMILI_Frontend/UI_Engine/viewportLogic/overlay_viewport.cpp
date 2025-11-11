#define GLM_ENABLE_EXPERIMENTAL

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
#include "CameraControl.hpp"
#include "RaycastPerform.hpp"
#include "TextureRendererTest.hpp"
#include "HtmlTextureRenderer.hpp"
#include "OverlayClickHandler.hpp"
#include "../../Engine/ThreeDScene.hpp"
#include "../../Engine/OpenGLContext.hpp"
#include "../../Engine/ThreeDObjectSelector.hpp"
#include "../../Engine/ThreeDInteractions/MeshTransform.hpp"
#include "../../Engine/Guizmo.hpp"
#include "../../WorldObjects/Camera/Camera.hpp"
#include "../../WorldObjects/Entities/ThreeDObject.hpp"
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

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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


// --------- Window and scene rendering ------------- 

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
{
	selector_ = new ThreeDObjectSelector();
	camera_control_ = new CameraControl(this);
	raycast_performer_ = new RaycastPerform(this, selector_);
	texture_renderer_test_ = new TextureRendererTest();
	click_handler_ = new OverlayClickHandler(this);
	
	// Initialize 3D modes
	normal_mode_ = new Normal_Mode();
	vertice_mode_ = new Vertice_Mode();
	face_mode_ = new Face_Mode();
	edge_mode_ = new Edge_Mode();
	
	// Initialize default mode to Normal Mode
	current_mode_ = normal_mode_;
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
	
	destroy();
}

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
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
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
	
	// Create the overlay window - ensure it receives mouse input
	hwnd_ = CreateWindowExW(
		WS_EX_NOPARENTNOTIFY,  // Don't notify parent, but DO receive input
		kOverlayClassName,
		L"OpenGL Overlay",
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP,
		x, y, width, height,
		parent,
		nullptr,
		GetModuleHandle(nullptr),
		this
	);
	
	if (!hwnd_) 
	{
		DWORD error = GetLastError();
		return false;
	}
	
	// Ensure window is top-level and receives input
	SetWindowPos(hwnd_, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	
	// Verify click handler is initialized
	if (!click_handler_) 
	{
		std::cerr << "[OverlayViewport] WARNING: Click handler not initialized!" << std::endl;
	}
	else 
	{
		std::cout << "[OverlayViewport] Click handler ready" << std::endl;
	}

	RECT window_rect;
	GetWindowRect(hwnd_, &window_rect);
	POINT top_left = {window_rect.left, window_rect.top};
	ScreenToClient(parent, &top_left);	
	initializeOpenGL();		
	return true;
}

void OverlayViewport::destroy() 
{
	shutdownImGui();
	
	if (gl_context_) {
		wglMakeCurrent(hdc_, gl_context_);
		wglMakeCurrent(nullptr, nullptr);
		wglDeleteContext(gl_context_);
		gl_context_ = nullptr;
	}
	
	if (hdc_) {
		ReleaseDC(hwnd_, hdc_);
		hdc_ = nullptr;
	}
	
	if (hwnd_) {
		DestroyWindow(hwnd_);
		hwnd_ = nullptr;
	}
}

void OverlayViewport::initializeOpenGL() 
{
	hdc_ = GetDC(hwnd_);
	
	PIXELFORMATDESCRIPTOR pfd = {};
	pfd.nSize = sizeof(pfd);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;
	pfd.cAlphaBits = 8;
	
	int pixel_format = ChoosePixelFormat(hdc_, &pfd);
	SetPixelFormat(hdc_, pixel_format, &pfd);
	
	HGLRC temp_context = wglCreateContext(hdc_);
	wglMakeCurrent(hdc_, temp_context);
	
	if (!gladLoadGL()) 
	{
		std::cerr << "[OverlayViewport] Failed to load GLAD" << std::endl;
		wglMakeCurrent(nullptr, nullptr);
		wglDeleteContext(temp_context);
		return;
	}
	
	int attribs[] = 
	{
		WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
		WGL_CONTEXT_MINOR_VERSION_ARB, 6,
		WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		0
	};
	
	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = 
	(PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
	
	if (wglCreateContextAttribsARB) 
	{
		gl_context_ = wglCreateContextAttribsARB(hdc_, 0, attribs);
		wglMakeCurrent(nullptr, nullptr);
		wglDeleteContext(temp_context);
		wglMakeCurrent(hdc_, gl_context_);
		
		std::cout << "[OverlayViewport] OpenGL " << glGetString(GL_VERSION) 
				  << " - " << glGetString(GL_RENDERER)
				  << " (Core Profile 4.6)" << std::endl;
	}
	else 
	{
		gl_context_ = temp_context;
		std::cout << "[OverlayViewport] OpenGL " << glGetString(GL_VERSION) 
				  << " - " << glGetString(GL_RENDERER)
				  << " (Legacy - fallback)" << std::endl;
	}
	
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	std::cout << "[OverlayViewport] OpenGL initialized for 3D scene rendering" << std::endl;
	
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
}

void OverlayViewport::initializeImGui() 
{
	if (imgui_initialized_) 
	{
		return;
	}
	
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;// Enable Gamepad Controls

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

void OverlayViewport::makeContextCurrent() {
	if (hdc_ && gl_context_) {
		wglMakeCurrent(hdc_, gl_context_);
	}
}

void OverlayViewport::releaseContext() {
	wglMakeCurrent(nullptr, nullptr);
}

void OverlayViewport::setPosition(int x, int y, int width, int height) 
{
	if (hwnd_) 
	{
		SetWindowPos(hwnd_, HWND_TOP, x, y, width, height, 0);
		width_ = width;
		height_ = height;
		
		if (gl_context_ && hdc_) 
		{
			wglMakeCurrent(hdc_, gl_context_);
			glViewport(0, 0, width, height);
			wglMakeCurrent(nullptr, nullptr);
		}
		
		if (texture_renderer_test_) 
		{
			texture_renderer_test_->resize(width, height);
		}
	}
}

void OverlayViewport::show(bool visible) 
{
	if (hwnd_) 
	{
		ShowWindow(hwnd_, visible ? SW_SHOW : SW_HIDE);
	}
}

bool OverlayViewport::isVisible() const {
	if (hwnd_) {
		return IsWindowVisible(hwnd_) != 0;
	}
	return false;
}

void OverlayViewport::render() 
{
	if (!hwnd_ || !gl_context_ || !rendering_enabled_) 
	{
		return;
	}
	
	// Process CEF message loop for off-screen rendering
	CefDoMessageLoopWork();
	
	wglMakeCurrent(hdc_, gl_context_);
	
	// Start ImGui frame
	if (imgui_initialized_) 
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
		
		// Create an invisible fullscreen window for ImGuizmo
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
			ImGuiWindowFlags_NoBringToFrontOnFocus);
	}
	
	renderScene();
	
	// Handle 3D world interactions (Gizmo manipulations for all modes)
	ThreeDWorldInteractions();
	
	if (texture_renderer_test_) {
		texture_renderer_test_->render();
	}
	
	if (imgui_initialized_) 
	{
		ImGui::End();
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}
	
	SwapBuffers(hdc_);
	
}

void OverlayViewport::renderScene() {

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, width_, height_);
	
	if (three_d_scene_) 
	{
		three_d_scene_->renderDirect(width_, height_);
		return;
	}
	
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	std::cout << "[OverlayViewport] WARNING: No 3D scene set" << std::endl;
}
// ----------- end of window and scene rendering ------------

// ------------- Viewport mode management -------------

void OverlayViewport::renderGuizmo() 
{
	// This function is deprecated - Guizmo rendering is now handled in ThreeDWorldInteractions()
	// Kept for backward compatibility
}

// -------------- Camera and overlay controls --------------
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
			
			case WM_ERASEBKGND:
				return 1;
			
			// ------ RAYCAST & SELECTION ------
			
			case WM_LBUTTONDOWN:
			{
				std::cout << "[OverlayViewport] WM_LBUTTONDOWN received" << std::endl;
				
				// Capture mouse to ensure we get all mouse events
				SetCapture(hwnd);
				
				// Get mouse coordinates
				int mouseX = LOWORD(lParam);
				int mouseY = HIWORD(lParam);
				
				std::cout << "[OverlayViewport] Click at: (" << mouseX << ", " << mouseY << ")" << std::endl;
				std::cout << "[OverlayViewport] ImGuizmo state - IsOver: " << ImGuizmo::IsOver() 
				          << " | IsUsing: " << ImGuizmo::IsUsing() << std::endl;
				
				// Check if we should process the click (only block when actively using gizmo)
				if (shouldProcessEvent && !ImGuizmo::IsUsing())
				{
					std::cout << "[OverlayViewport] Processing click handler" << std::endl;
					
					if (overlay->click_handler_) 
					{
						overlay->click_handler_->handle();
					}
					else 
					{
						std::cerr << "[OverlayViewport] ERROR: No click handler available!" << std::endl;
					}
				}
				else 
				{
					std::cout << "[OverlayViewport] Click blocked - ImGuizmo is being manipulated" << std::endl;
				}
				
				return 0;
			}
			
			case WM_LBUTTONUP:
			{
				// Release mouse capture
				ReleaseCapture();
				return 0;
			}
					
			// ------ CAMERA CONTROLS ------
			
			case WM_MOUSEWHEEL: 
			{
				// Only handle camera zoom if NOT over ImGuizmo
				if (!ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
				{
					POINT cursor_pos;
					GetCursorPos(&cursor_pos);
					ScreenToClient(hwnd, &cursor_pos);
					
					overlay->camera_control_->onMouseWheel(wParam);
				}
				return 0;
			}
			
			case WM_MBUTTONDOWN: 
			{
				// Only handle camera pan if NOT over ImGuizmo
				if (!ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
				{
					overlay->camera_control_->onMiddleButtonDown();
				}
				return 0;
			}
			
			case WM_MBUTTONUP: 
			{
				overlay->camera_control_->onMiddleButtonUp();
				return 0;
			}
			
			case WM_MOUSEMOVE: 
			{
				// ImGui already processed this event at the top of WndProc
				// Only handle camera movement if not over/using gizmo
				if (!ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
				{
					overlay->camera_control_->onMouseMove(wParam, lParam);
				}
				// Always trigger redraw for ImGuizmo hover state updates
				InvalidateRect(hwnd, nullptr, FALSE);
				return 0;
			}
			
			// ------ MODE SWITCHING -----
			
			case WM_KEYDOWN:
			{
				// Check for numeric keys 1-4 to switch modes
				// Mapping: 1=Normal, 2=Edge, 3=Vertice, 4=Face
				if (wParam == '1')
				{
					overlay->switchModeByKey(1); // Normal Mode
					std::cout << "[OverlayViewport] Switched to Normal Mode" << std::endl;
					return 0;
				}
				else if (wParam == '2')
				{
					overlay->switchModeByKey(2); // Edge Mode
					std::cout << "[OverlayViewport] Switched to Edge Mode" << std::endl;
					return 0;
				}
				else if (wParam == '3')
				{
					overlay->switchModeByKey(3); // Vertice Mode
					std::cout << "[OverlayViewport] Switched to Vertice Mode" << std::endl;
					return 0;
				}
				else if (wParam == '4')
				{
					overlay->switchModeByKey(4); // Face Mode
					std::cout << "[OverlayViewport] Switched to Face Mode" << std::endl;
					return 0;
				}
				break;
			}
		}
	}
	
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ------- Raycast & Selection Implementation -------

void OverlayViewport::performRaycast(int mouseX, int mouseY) 
{
	if (raycast_performer_) {
		raycast_performer_->performRaycast(mouseX, mouseY);
	}
}

// --------- 3D mode management ---------

void OverlayViewport::setModelingMode(ThreeDMode* mode)
{
	if (mode)
	{
		current_mode_ = mode;
		std::cout << "[OverlayViewport] Mode changed to: " << mode->getName() << std::endl;
	}
}

void OverlayViewport::setMultipleSelectedObjects(const std::list<ThreeDObject*>& objects) 
{ 
	multiple_selected_objects_ = objects; 
	std::cout << "[OverlayViewport] setMultipleSelectedObjects called with " 
	          << objects.size() << " objects" << std::endl;
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

// --------- Object Manipulation ----------- //

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
	
	// For overlay, the ImGui window is fullscreen, so position is (0, 0)
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
			was_using_gizmo_last_frame_
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
			true
		);
	}
	else if (current_mode_ == edge_mode_)
	{
		// EdgeTransform needs ThreeDWindow* parameter which we don't have in overlay
		// We'll need to adapt EdgeTransform or create an overlay-specific version
		EdgeTransform::manipulateEdges(
			three_d_scene_,
			multiple_selected_edges_,
			oglChildPos,
			oglChildSize,
			was_using_gizmo_last_frame_,
			nullptr  // ThreeDWindow pointer - NULL for overlay (will need modification)
		);
	}
	
}