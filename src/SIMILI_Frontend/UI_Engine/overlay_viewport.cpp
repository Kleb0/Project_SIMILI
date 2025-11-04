#define GLM_ENABLE_EXPERIMENTAL

#include "overlay_viewport.hpp"
#include "../../Engine/ThreeDScene.hpp"
#include "../../Engine/OpenGLContext.hpp"
#include "../../WorldObjects/Camera/Camera.hpp"
#include <iostream>
#include <glad/glad.h>

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

OverlayViewport::OverlayViewport()
	: hwnd_(nullptr)
	, parent_(nullptr)
	, hdc_(nullptr)
	, gl_context_(nullptr)
	, width_(800)
	, height_(600)
	, three_d_scene_(nullptr)
	, rendering_enabled_(true)
	, is_dragging_(false)
{
	last_mouse_pos_.x = 0;
	last_mouse_pos_.y = 0;
}

OverlayViewport::~OverlayViewport() {
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
	int attribs[] = {
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
	
	renderScene();
	
	SwapBuffers(hdc_);
	
	// Don't call wglMakeCurrent(nullptr, nullptr) - it's expensive
	// The context will be properly released when needed
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
			
			// ===== CAMERA CONTROLS =====
			
			case WM_MOUSEWHEEL: 
			{
				int delta = GET_WHEEL_DELTA_WPARAM(wParam);
				float wheel = delta / 120.0f; // Normalize
				
				if (overlay->three_d_scene_) {
					Camera* cam = overlay->three_d_scene_->getActiveCamera();
					if (cam && cam->isSoftwareCamera()) {
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

				if (overlay->is_dragging_) {
					overlay->is_dragging_ = false;
					ReleaseCapture();
				}
				return 0;
			}
			
			case WM_MOUSEMOVE: 
			{
				if (overlay->is_dragging_ && overlay->three_d_scene_) {
					Camera* cam = overlay->three_d_scene_->getActiveCamera();
					if (cam && cam->isSoftwareCamera()) {
						POINT current_pos;
						GetCursorPos(&current_pos);
						ScreenToClient(hwnd, &current_pos);
						
						float deltaX = static_cast<float>(current_pos.x - overlay->last_mouse_pos_.x);
						float deltaY = static_cast<float>(current_pos.y - overlay->last_mouse_pos_.y);
						
						if (deltaX != 0.0f || deltaY != 0.0f) {
							bool shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
							
							if (!shiftPressed) {
								cam->prepareOrbit();
								cam->orbitAroundTarget(deltaX, deltaY);
							} else {
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

