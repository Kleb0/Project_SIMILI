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

#include "overlay_viewport.hpp"
#include "../../Engine/ThreeDScene.hpp"
#include "../../Engine/OpenGLContext.hpp"
#include "../../Engine/ThreeDObjectSelector.hpp"
#include "../../WorldObjects/Camera/Camera.hpp"
#include "../../WorldObjects/Entities/ThreeDObject.hpp"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif

typedef HGLRC (WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC hDC, HGLRC hShareContext, const int *attribList);

namespace {
	const wchar_t* kOverlayClassName = L"SIMILI_OpenGL_Overlay";
}

OverlayViewport::OverlayViewport() : hwnd_(nullptr) 
	, parent_(nullptr)
	, hdc_(nullptr)
	, gl_context_(nullptr)
	, width_(800)
	, height_(600)
	, three_d_scene_(nullptr)
	, rendering_enabled_(true)
	, is_dragging_(false)
	, imgui_initialized_(false)
	, selector_(nullptr)
	, current_guizmo_operation_(ImGuizmo::TRANSLATE)
	, current_guizmo_mode_(ImGuizmo::WORLD)
{
	last_mouse_pos_.x = 0;
	last_mouse_pos_.y = 0;
	selector_ = new ThreeDObjectSelector();
}

OverlayViewport::~OverlayViewport() {
	if (selector_) {
		delete selector_;
		selector_ = nullptr;
	}
	destroy();
}

bool OverlayViewport::create(HWND parent, int x, int y, int width, int height) {
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
	if (!class_registered) {
		if (!RegisterClassExW(&wc)) {
			DWORD err = GetLastError();
			std::cerr << "[OverlayViewport] Failed to register window class. Error: " << err << std::endl;
			return false;
		}
		class_registered = true;
		std::cout << "[OverlayViewport] Window class registered" << std::endl;
	}
	
	// Create as child window
	std::cout << "[OverlayViewport] Creating child window with parent: " << parent 
			  << " at (" << x << "," << y << ") size " << width << "x" << height << std::endl;
	
	hwnd_ = CreateWindowExW(
		0, 
		kOverlayClassName,
		L"OpenGL Overlay",
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		x, y, width, height,
		parent,
		nullptr,
		GetModuleHandle(nullptr),
		this
	);
	
	if (!hwnd_) {
		DWORD error = GetLastError();
		std::cerr << "[OverlayViewport] Failed to create overlay window. Error: " << error << std::endl;
		return false;
	}
	
	std::cout << "[OverlayViewport] Window created successfully: " << hwnd_ << std::endl;

	SetWindowPos(hwnd_, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	
	RECT window_rect;
	GetWindowRect(hwnd_, &window_rect);
	POINT top_left = {window_rect.left, window_rect.top};
	ScreenToClient(parent, &top_left);
	std::cout << "[OverlayViewport] Actual client position: (" << top_left.x << ", " << top_left.y << ")" << std::endl;
		
	initializeOpenGL();
	
	std::cout << "[OverlayViewport] Created at (" << x << "," << y << ") size " << width << "x" << height << std::endl;
	
	return true;
}

void OverlayViewport::destroy() {
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
	
	// Create OpenGL 4.6 core profile context
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
}

void OverlayViewport::initializeImGui() 
{
	if (imgui_initialized_) {
		std::cout << "[OverlayViewport] ImGui already initialized" << std::endl;
		return;
	}
	
	std::cout << "[OverlayViewport] Initializing ImGui..." << std::endl;
	
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	
	// Setup Dear ImGui style
	ImGui::StyleColorsDark();
	
	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd_);
	ImGui_ImplOpenGL3_Init("#version 460");
	
	// Initialize ImGuizmo
	ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
	
	imgui_initialized_ = true;
	std::cout << "[OverlayViewport] ImGui and ImGuizmo initialized successfully" << std::endl;
}

void OverlayViewport::shutdownImGui() 
{
	if (!imgui_initialized_) {
		return;
	}
	
	std::cout << "[OverlayViewport] Shutting down ImGui..." << std::endl;
	
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
	
	imgui_initialized_ = false;
	std::cout << "[OverlayViewport] ImGui shutdown complete" << std::endl;
}

void OverlayViewport::makeContextCurrent() {
	if (hdc_ && gl_context_) {
		wglMakeCurrent(hdc_, gl_context_);
	}
}

void OverlayViewport::releaseContext() {
	wglMakeCurrent(nullptr, nullptr);
}

void OverlayViewport::setPosition(int x, int y, int width, int height) {
	if (hwnd_) {
		SetWindowPos(hwnd_, HWND_TOP, x, y, width, height, 0);
		width_ = width;
		height_ = height;
		
		if (gl_context_ && hdc_) {
			wglMakeCurrent(hdc_, gl_context_);
			glViewport(0, 0, width, height);
			wglMakeCurrent(nullptr, nullptr);
		}
	}
}

void OverlayViewport::show(bool visible) {
	if (hwnd_) {
		ShowWindow(hwnd_, visible ? SW_SHOW : SW_HIDE);
	}
}

bool OverlayViewport::isVisible() const {
	if (hwnd_) {
		return IsWindowVisible(hwnd_) != 0;
	}
	return false;
}

void OverlayViewport::render() {
	if (!hwnd_ || !gl_context_ || !rendering_enabled_) {
		return;
	}
	
	wglMakeCurrent(hdc_, gl_context_);
	
	// Start ImGui frame
	if (imgui_initialized_) 
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}
	
	renderScene();
	
	// Render ImGui
	if (imgui_initialized_) 
	{
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
		
		// Render Guizmo for selected object
		renderGuizmo();
		
		return;
	}
	
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	std::cout << "[OverlayViewport] WARNING: No 3D scene set" << std::endl;
}


// ------------- Viewport mode management -------------

void OverlayViewport::renderGuizmo() 
{
	if (!selector_ || !three_d_scene_ || !imgui_initialized_) 
	{
		return;
	}
	
	ThreeDObject* selectedObject = selector_->getSelectedObject();
	if (!selectedObject) {
		return; // No object selected, no gizmo to render
	}
	
	Camera* camera = three_d_scene_->getActiveCamera();
	if (!camera) {
		return;
	}
	
	// Get view and projection matrices
	glm::mat4 view = three_d_scene_->getViewMatrix();
	glm::mat4 projection = three_d_scene_->getProjectionMatrix();
	
	// Get object's current model matrix
	glm::mat4 model = selectedObject->getModelMatrix();
	
	// Setup ImGuizmo
	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
	
	// Set the rect to match the entire viewport
	ImGuizmo::SetRect(0, 0, static_cast<float>(width_), static_cast<float>(height_));
	
	// Render the gizmo
	glm::mat4 delta = glm::mat4(1.0f);
	
	ImGuizmo::Manipulate(
		glm::value_ptr(view),
		glm::value_ptr(projection),
		current_guizmo_operation_,
		current_guizmo_mode_,
		glm::value_ptr(model),
		glm::value_ptr(delta),
		nullptr  // No snap for now
	);
	
	if (ImGuizmo::IsUsing()) 
	{
		glm::vec3 position, scale, skew;
		glm::vec4 perspective;
		glm::quat rotation;
		
		glm::decompose(model, scale, rotation, position, skew, perspective);
		
		selectedObject->setPosition(position);
		selectedObject->setScale(scale);
		selectedObject->rotation = rotation;
		
		std::cout << "[OverlayViewport] Guizmo manipulation: " 
				  << selectedObject->getName() 
				  << " Pos(" << position.x << ", " << position.y << ", " << position.z << ")"
				  << std::endl;
	}
}

// -------------- Camera Controls --------------
LRESULT CALLBACK OverlayViewport::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) 
{
	// Pass events to ImGui first
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
		return true;

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
			
			// ===== RAYCAST & SELECTION =====
			
			case WM_LBUTTONDOWN:
			{
				POINT cursor_pos;
				GetCursorPos(&cursor_pos);
				ScreenToClient(hwnd, &cursor_pos);
				
				overlay->performRaycast(cursor_pos.x, cursor_pos.y);
				return 0;
			}
			
			// ===== GUIZMO CONTROLS (Keyboard shortcuts) =====
			
			case WM_KEYDOWN:
			{
				switch (wParam)
				{
					case 'G':  // Translate mode
						overlay->setGuizmoOperation(ImGuizmo::TRANSLATE);
						std::cout << "[OverlayViewport] Guizmo mode: TRANSLATE" << std::endl;
						InvalidateRect(hwnd, nullptr, FALSE);
						return 0;
						
					case 'R':  // Rotate mode
						overlay->setGuizmoOperation(ImGuizmo::ROTATE);
						std::cout << "[OverlayViewport] Guizmo mode: ROTATE" << std::endl;
						InvalidateRect(hwnd, nullptr, FALSE);
						return 0;
						
					case 'S':  // Scale mode
						overlay->setGuizmoOperation(ImGuizmo::SCALE);
						std::cout << "[OverlayViewport] Guizmo mode: SCALE" << std::endl;
						InvalidateRect(hwnd, nullptr, FALSE);
						return 0;
						
					case VK_ESCAPE:  // Clear selection
						if (overlay->selector_) {
							overlay->selector_->clearTarget();
							std::cout << "[OverlayViewport] Selection cleared" << std::endl;
							InvalidateRect(hwnd, nullptr, FALSE);
						}
						return 0;
				}
				break;
			}
			
			// ===== CAMERA CONTROLS =====
			
			case WM_MOUSEWHEEL: 
			{
				int delta = GET_WHEEL_DELTA_WPARAM(wParam);
				float wheel = delta / 120.0f; // Normalize
				
				if (overlay->three_d_scene_) 
				{
					Camera* cam = overlay->three_d_scene_->getActiveCamera();
					if (cam && cam->isSoftwareCamera()) 
					{
						cam->moveForward(wheel * 0.5f);
						InvalidateRect(hwnd, nullptr, FALSE);
					}
				}
				return 0;
			}
			
			case WM_MBUTTONDOWN: 
			{
				SetCapture(hwnd);
				overlay->is_dragging_ = true;
				GetCursorPos(&overlay->last_mouse_pos_);
				ScreenToClient(hwnd, &overlay->last_mouse_pos_);
				return 0;
			}
			
			case WM_MBUTTONUP: 
			{

				if (overlay->is_dragging_) 
				{
					overlay->is_dragging_ = false;
					ReleaseCapture();
				}
				return 0;
			}
			
			case WM_MOUSEMOVE: 
			{
				if (overlay->is_dragging_ && overlay->three_d_scene_) 
				{
					Camera* cam = overlay->three_d_scene_->getActiveCamera();
					if (cam && cam->isSoftwareCamera()) {
						POINT current_pos;
						GetCursorPos(&current_pos);
						ScreenToClient(hwnd, &current_pos);
						
						float deltaX = static_cast<float>(current_pos.x - overlay->last_mouse_pos_.x);
						float deltaY = static_cast<float>(current_pos.y - overlay->last_mouse_pos_.y);
						
						if (deltaX != 0.0f || deltaY != 0.0f) {
							bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
							
							if (!shiftPressed) 
							{
								cam->prepareOrbit();
								cam->orbitAroundTarget(deltaX, deltaY);
							} 
							else 
						{
								cam->lateralMovement(deltaX, deltaY);
							}
							
							InvalidateRect(hwnd, nullptr, FALSE);
						}
						
						overlay->last_mouse_pos_ = current_pos;
					}
				}
				return 0;
			}
		}
	}
	
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ===== Raycast & Selection Implementation =====

void OverlayViewport::performRaycast(int mouseX, int mouseY) 
{
	if (!three_d_scene_ || !selector_) 
	{
		std::cout << "[OverlayViewport] Raycast impossible: scene or selector not initialized" << std::endl;
		return;
	}
	
	Camera* camera = three_d_scene_->getActiveCamera();
	if (!camera) 
	{
		std::cout << "[OverlayViewport] No active camera for raycast" << std::endl;
		return;
	}
	
	std::cout << "\n========== RAYCAST DEBUG ==========" << std::endl;
	std::cout << "[OverlayViewport] Mouse: (" << mouseX << ", " << mouseY << ")" << std::endl;
	std::cout << "[OverlayViewport] Viewport: " << width_ << "x" << height_ << std::endl;
	
	// Get view and projection matrices
	glm::mat4 view = three_d_scene_->getViewMatrix();
	glm::mat4 projection = three_d_scene_->getProjectionMatrix();
	
	std::cout << "[OverlayViewport] Camera position: (" 
	          << camera->getPosition().x << ", " 
	          << camera->getPosition().y << ", " 
	          << camera->getPosition().z << ")" << std::endl;
	
	// Get all objects in the scene
	const auto& listRef = three_d_scene_->getObjectsRef();
	
	std::vector<ThreeDObject*> objects;
	objects.reserve(listRef.size());
	
	for (auto* obj : listRef) 
	{
		if (obj) 
		{
			std::cout << "[OverlayViewport] Object: " << obj->getName() 
			          << " | Selectable: " << (obj->isSelectable() ? "YES" : "NO")
			          << " | Pos: (" << obj->getPosition().x << ", " 
			          << obj->getPosition().y << ", " 
			          << obj->getPosition().z << ")" << std::endl;
			objects.push_back(obj);
		}
	}
	
	if (objects.empty()) 
	{
		std::cout << "[OverlayViewport] ERROR: No objects in the scene" << std::endl;
		return;
	}
	
	std::cout << "[OverlayViewport] Starting raycast with " << objects.size() << " objects..." << std::endl;
	selector_->pickUpMesh(mouseX, mouseY, width_, height_, view, projection, objects);
	
	ThreeDObject* selected = selector_->getSelectedObject();
	
	if (selected) 
	{
		std::cout << "\n*** OBJECT CLICKED ***" << std::endl;
		std::cout << "  Name: " << selected->getName() << std::endl;
		std::cout << "  Position: (" 
				  << selected->getPosition().x << ", " 
				  << selected->getPosition().y << ", " 
				  << selected->getPosition().z << ")" << std::endl;
		
		// Trigger a redraw to show the gizmo
		InvalidateRect(hwnd_, nullptr, FALSE);
	} 
	else 
	{
		std::cout << "Clicked empty space (no object selected)" << std::endl;
	}
	std::cout << "===================================\n" << std::endl;
	
	// Note: Do NOT clear selection here - we want to keep it for the gizmo
	// selector_->clearTarget();
}
