#include "ui_handler.hpp"
#include "viewportLogic/HtmlTextureRenderer.hpp"
#include "../../Engine/ThreeDScene.hpp"
#include "../../Engine/OpenGLContext.hpp"
#include "../../WorldObjects/Camera/Camera.hpp"
#include "../../WorldObjects/Mesh/Mesh.hpp"
#include "../../Engine/PrimitivesCreation/CreatePrimitive.hpp"
#include <iostream>
#include <commctrl.h>  
#include <glm/glm.hpp>

#pragma comment(lib, "comctl32.lib")

UIHandler::UIHandler() : parent_hwnd_(nullptr), timer_id_(0),
	last_viewport_update_time_(0), last_viewport_x_(0), last_viewport_y_(0), 
	last_viewport_width_(0), last_viewport_height_(0), three_d_scene_(nullptr),
	renderer_(nullptr), main_camera_(nullptr), cube_mesh_ptr_(nullptr), scene_initialized_(false)
{
}

UIHandler::~UIHandler() 
{
	stopRenderTimer();
}

CefRefPtr<CefDisplayHandler> UIHandler::GetDisplayHandler() 
{
	return this;
}

CefRefPtr<CefLifeSpanHandler> UIHandler::GetLifeSpanHandler() 
{
	return this;
}

CefRefPtr<CefLoadHandler> UIHandler::GetLoadHandler() 
{
	return this;
}

CefRefPtr<CefKeyboardHandler> UIHandler::GetKeyboardHandler()
{
	return this;
}

void UIHandler::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) 
{
	std::string title_str = title.ToString();
	
	if (title_str.find("VIEWPORT_RESIZE:") == 0) 
	{

		std::string coords = title_str.substr(16); 
		
		int js_x, js_y, width, height;
		float dpiScale = 1.0f;
		std::istringstream iss(coords);
		char comma;
		
		if (iss >> js_x >> comma >> js_y >> comma >> width >> comma >> height) 
		{
			if (iss >> comma >> dpiScale) 
			{
				// DPI scale read successfully
			}
			
			if (overlay_viewport_ && parent_hwnd_ && browser) 
			{
				int final_x = static_cast<int>(js_x * dpiScale);
				int final_y = static_cast<int>(js_y * dpiScale);
				int final_width = static_cast<int>(width * dpiScale);
				int final_height = static_cast<int>(height * dpiScale);
				
				DWORD current_time = GetTickCount();
				bool position_changed = (abs(final_x - last_viewport_x_) > 2 || 
				                        abs(final_y - last_viewport_y_) > 2 ||
				                        abs(final_width - last_viewport_width_) > 2 ||
				                        abs(final_height - last_viewport_height_) > 2);
				bool time_elapsed = (current_time - last_viewport_update_time_) > 16;
				
				if (position_changed || time_elapsed || !overlay_viewport_->isVisible()) 
				{
					overlay_viewport_->setPosition(final_x, final_y, final_width, final_height);
					
					last_viewport_update_time_ = current_time;
					last_viewport_x_ = final_x;
					last_viewport_y_ = final_y;
					last_viewport_width_ = final_width;
					last_viewport_height_ = final_height;
					
					if (!overlay_viewport_->isVisible()) 
					{
						overlay_viewport_->show(true);
						std::cout << "[UIHandler] Overlay now visible" << std::endl;
					}
				}
			}
		}
	}
}

void UIHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) 
{
	CEF_REQUIRE_UI_THREAD();
	browser_list_.push_back(browser);
}

bool UIHandler::DoClose(CefRefPtr<CefBrowser> browser) 
{
	CEF_REQUIRE_UI_THREAD();
	return false;
}

void UIHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) 
{
	CEF_REQUIRE_UI_THREAD();

	for (auto it = browser_list_.begin(); it != browser_list_.end(); ++it) 
	{
		if ((*it)->IsSame(browser)) 
		{
			browser_list_.erase(it);
			break;
		}
	}

	if (browser_list_.empty()) 
	{
		CefQuitMessageLoop();
	}
}

void UIHandler::OnLoadError(CefRefPtr<CefBrowser> browser,
CefRefPtr<CefFrame> frame, ErrorCode errorCode, const CefString& errorText, const CefString& failedUrl) 
{
	CEF_REQUIRE_UI_THREAD();

	if (errorCode == ERR_ABORTED)
		return;

	std::stringstream ss;

	ss << "<html><body bgcolor=\"white\">"
		"<h2>Failed to load URL "
	   << std::string(failedUrl) << " with error " << std::string(errorText)
	   << " (" << errorCode << ").</h2></body></html>";

	frame->LoadURL(CefString("data:text/html," + ss.str()));
}

void UIHandler::CloseAllBrowsers(bool force_close) 
{
	if (!CefCurrentlyOn(TID_UI)) 
	{
		return;
	}

	for (auto it = browser_list_.begin(); it != browser_list_.end(); ++it) 
	{
		(*it)->GetHost()->CloseBrowser(force_close);
	}
}

void UIHandler::createOverlayViewport(HWND parent_hwnd) 
{
	parent_hwnd_ = parent_hwnd;
		
	if (!overlay_viewport_) 
	{
		overlay_viewport_ = std::make_unique<OverlayViewport>();
	}

	RECT client_rect;
	GetClientRect(parent_hwnd, &client_rect);
	
	int window_width = client_rect.right - client_rect.left;
	int window_height = client_rect.bottom - client_rect.top;	

	// Create overlay with placeholder dimensions (will be updated by JavaScript)
	int overlay_x = 0;
	int overlay_y = 0;
	int overlay_w = 100;
	int overlay_h = 100;
	
	overlay_viewport_->create(parent_hwnd, overlay_x, overlay_y, overlay_w, overlay_h);
	// Hide overlay until JavaScript sends proper dimensions
	overlay_viewport_->show(false);
	
	// Pass 3D scene to overlay
	if (three_d_scene_) 
	{
		overlay_viewport_->setThreeDScene(three_d_scene_);
	}
	
	// CRITICAL: Initialize scene objects NOW that we have an OpenGL context
	initializeSceneObjects();
		
	// Install window subclass to handle resize
	SetWindowSubclass(parent_hwnd, ParentWindowProc, 0, reinterpret_cast<DWORD_PTR>(this));
	
	// Start render timer (60 FPS)
	startRenderTimer();
}

void UIHandler::updateOverlayPosition() 
{
	if (overlay_viewport_ && parent_hwnd_) 
	{
		RECT client_rect;
		GetClientRect(parent_hwnd_, &client_rect);
		
		int window_width = client_rect.right - client_rect.left;
		int window_height = client_rect.bottom - client_rect.top;
		
		int left_panel_width = (int)(window_width * 0.15f);
		int right_panel_width = (int)(window_width * 0.15f);
		int viewport_width = window_width - left_panel_width - right_panel_width;
		int viewport_height = (int)(window_height * 0.60f);
		
		// Add 5px inset on all sides
		const int inset = 5;
		overlay_viewport_->setPosition(
			left_panel_width + inset, 
			inset, 
			viewport_width - (2 * inset), 
			viewport_height - (2 * inset)
		);
	}
}

static VOID CALLBACK RenderTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) 
{
	// Timer callback - trigger redraw
	UIHandler* handler = reinterpret_cast<UIHandler*>(idEvent);

	if (handler && handler->getOverlay()) 
	{
		// Process CEF message loop for off-screen browser
		CefDoMessageLoopWork();
		
		// Trigger viewport redraw
		InvalidateRect(handler->getOverlay()->getHandle(), nullptr, FALSE);
	}
}

void UIHandler::startRenderTimer() 
{
	if (timer_id_ == 0 && overlay_viewport_) 
	{
		// 60 FPS = ~16ms
		timer_id_ = SetTimer(nullptr, reinterpret_cast<UINT_PTR>(this), 16, RenderTimerProc);
	}
}

void UIHandler::stopRenderTimer() 
{
	if (timer_id_ != 0) 
	{
		KillTimer(nullptr, timer_id_);
		timer_id_ = 0;
	}
}

void UIHandler::enableOverlayRendering(bool enable) 
{
	if (overlay_viewport_) 
	{
		overlay_viewport_->enableRendering(enable);
	}
}

bool UIHandler::isOverlayRenderingEnabled() const 
{
	if (overlay_viewport_) 
	{
		return overlay_viewport_->isRenderingEnabled();
	}
	return false;
}

LRESULT CALLBACK UIHandler::ParentWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) 
{
	UIHandler* handler = reinterpret_cast<UIHandler*>(dwRefData);
	
	switch (msg) 
	{
		case WM_SIZE:
			// Don't call updateOverlayPosition() here - JavaScript will send new dimensions
			// via layout_resizer.js when window resizes
			if (handler && handler->overlay_viewport_) 
			{
				SetWindowPos(handler->overlay_viewport_->getHandle(), HWND_TOP, 0, 0, 0, 0,
				             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
			}
			break;
		
		case WM_ACTIVATE:
		case WM_WINDOWPOSCHANGED:
			if (handler && handler->overlay_viewport_) 
			{
				SetWindowPos(handler->overlay_viewport_->getHandle(), HWND_TOP, 0, 0, 0, 0,
				             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
			}
			break;
			
		case WM_NCDESTROY:
			// Remove subclass before window is destroyed
			RemoveWindowSubclass(hwnd, ParentWindowProc, uIdSubclass);
			break;
	}
	
	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void UIHandler::setSceneObjects(OpenGLContext* renderer, ThreeDScene* scene, Camera* camera, Mesh** cubeMesh) {
	renderer_ = renderer;
	three_d_scene_ = scene;
	main_camera_ = camera;
	cube_mesh_ptr_ = cubeMesh;
	scene_initialized_ = false;
	
	std::cout << "[UIHandler] Scene objects stored for deferred initialization" << std::endl;
}

void UIHandler::initializeSceneObjects() 
{
	if (scene_initialized_ || !three_d_scene_ || !main_camera_) 
	{
		std::cout << "[UIHandler] Scene already initialized or missing objects" << std::endl;
		return;
	}
	
	if (!overlay_viewport_) 
	{
		std::cerr << "[UIHandler] ERROR: Overlay viewport not created yet!" << std::endl;
		return;
	}
	
	std::cout << "[UIHandler] Initializing scene objects with OpenGL context..." << std::endl;
	
	try 
	{
		// CRITICAL: Make overlay's OpenGL context current before any GL calls
		overlay_viewport_->makeContextCurrent();
		
		// Initialize scene (will compile shaders, create grid VAO, etc.)
		// NOTE: We DON'T initialize renderer_ because overlay renders directly, no FBO needed
		three_d_scene_->initizalize();
		
		main_camera_->initialize();
		
		// Create cube mesh if pointer is provided
		if (cube_mesh_ptr_) 
		{
			*cube_mesh_ptr_ = Primitives::CreateCubeMesh(1.0f, glm::vec3(0.0f, 0.0f, 0.0f), "Cube", true);
			(*cube_mesh_ptr_)->initialize();
			
			three_d_scene_->addObject(*cube_mesh_ptr_);
		}
		
		three_d_scene_->addObject(main_camera_);
		three_d_scene_->setActiveCamera(main_camera_);
		
		scene_initialized_ = true;
		
		// Context will remain current for rendering
		
	} catch (const std::exception& e) {
		std::cerr << "[UIHandler] ERROR during scene initialization: " << e.what() << std::endl;
	} catch (...) {
		std::cerr << "[UIHandler] UNKNOWN ERROR during scene initialization" << std::endl;
	}
}

// ---------- Keyboard Handler Implementation ---------

bool UIHandler::OnPreKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event,
	CefEventHandle os_event, bool* is_keyboard_shortcut)
{

	
	if (event.type != KEYEVENT_RAWKEYDOWN && event.type != KEYEVENT_KEYDOWN) 
	{
		return false;
	}
	
	// Intercept keyboard events and forward to overlay's HTML texture browser
	if (overlay_viewport_) 
	{
		HtmlTextureRenderer* html_renderer = overlay_viewport_->getHtmlTextureRenderer();
		
		if (html_renderer) 
		{
			html_renderer->sendKeyEvent(event);
			
			return true;
		}
		else 
		{
			std::cout << "[UIHandler] No HTML renderer available" << std::endl;
		}
	}
	
	std::cout << "[UIHandler] OnPreKeyEvent - no overlay, passing through" << std::endl;
	return false;
}

bool UIHandler::OnKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event,
CefEventHandle os_event)
{

	return false;
}
