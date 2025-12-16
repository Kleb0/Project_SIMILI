#include "ui_handler.hpp"
#include "viewportLogic/HTMLTextureRenderer/HtmlTextureRenderer.hpp"
#include "viewportLogic/HTMLTextureRenderer/SlotTexture.hpp"
#include "viewportLogic/KeyManagement/KeyManager.hpp"
#include "../../Engine/ThreeDScene.hpp"
#include "../../Engine/OpenGLContext.hpp"
#include "../../WorldObjects/Camera/Camera.hpp"
#include "../../WorldObjects/Mesh/Mesh.hpp"
#include "../../Engine/PrimitivesCreation/CreatePrimitive.hpp"
#include "../../UI/ThreeDModes/ThreeDMode.hpp"
#include "../../UI/ThreeDModes/Normal_Mode.hpp"
#include "../../UI/ThreeDModes/Vertice_Mode.hpp"
#include "../../UI/ThreeDModes/Face_Mode.hpp"
#include "../../UI/ThreeDModes/Edge_Mode.hpp"
#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include <iostream>
#include <commctrl.h>  
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <windows.h>
#include <timeapi.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winmm.lib")

UIHandler::UIHandler() : parent_hwnd_(nullptr), timer_id_(0),
	last_viewport_update_time_(0), last_viewport_x_(0), last_viewport_y_(0), 
	last_viewport_width_(0), last_viewport_height_(0), three_d_scene_(nullptr),
	renderer_(nullptr), main_camera_(nullptr), cube_mesh_ptr_(nullptr), scene_initialized_(false),
	render_message_router_(nullptr)
{
}

UIHandler::~UIHandler() 
{
	stopRenderTimer();
}

CefRefPtr<CefBrowserProcessHandler> UIHandler::GetBrowserProcessHandler() 
{
	return this;
}

CefRefPtr<CefRenderProcessHandler> UIHandler::GetRenderProcessHandler() 
{
	return this;
}

void UIHandler::OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine> command_line) 
{
	// Disable GPU acceleration to avoid conflicts with OpenGL overlay
	command_line->AppendSwitch("disable-gpu");
	command_line->AppendSwitch("disable-gpu-compositing");
	command_line->AppendSwitch("disable-software-rasterizer");
	command_line->AppendSwitch("disable-gpu-shader-disk-cache");
	command_line->AppendSwitch("disable-gpu-sandbox");
	command_line->AppendSwitch("in-process-gpu");
	command_line->AppendSwitch("disable-features=VizDisplayCompositor");
	
	// Performance optimizations
	command_line->AppendSwitch("disable-smooth-scrolling");
	command_line->AppendSwitch("disable-frame-rate-limit");
	command_line->AppendSwitchWithValue("max-gum-fps", "60");
	
	// Disable animations that can cause lag during resize
	command_line->AppendSwitch("disable-renderer-backgrounding");
	command_line->AppendSwitch("disable-backgrounding-occluded-windows");
}

void UIHandler::OnContextInitialized() 
{
}

void UIHandler::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) 
{
	if (!render_message_router_) 
	{
		CefMessageRouterConfig config;
		render_message_router_ = CefMessageRouterRendererSide::Create(config);
		std::cout << "[UIHandler] Message router created in render process" << std::endl;
	}
	
	if (render_message_router_) 
	{
		render_message_router_->OnContextCreated(browser, frame, context);
		std::cout << "[UIHandler] Context created for frame: " << frame->GetURL().ToString() << std::endl;
	}
}

void UIHandler::OnContextReleased(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) 
{
	if (render_message_router_) 
	{
		render_message_router_->OnContextReleased(browser, frame, context);
	}
}

bool UIHandler::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
	CefProcessId source_process, CefRefPtr<CefProcessMessage> message) 
{
	if (render_message_router_) 
	{
		return render_message_router_->OnProcessMessageReceived(browser, frame, source_process, message);
	}
	return false;
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
					
					// Ensure proper Z-order after resize
					overlay_viewport_->ensureProperZOrder();
					
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

	int overlay_x = 0;
	int overlay_y = 0;
	int overlay_w = 100;
	int overlay_h = 100;
	
	overlay_viewport_->create(parent_hwnd, overlay_x, overlay_y, overlay_w, overlay_h);
	
	overlay_viewport_->setUIHandler(this);
	
	if (three_d_scene_) 
	{
		overlay_viewport_->setThreeDScene(three_d_scene_);
		std::cout << "[UIHandler] 3D Scene passed to overlay viewport" << std::endl;
	}
	else
	{
		std::cout << "[UIHandler] WARNING: No 3D scene available to pass to overlay!" << std::endl;
	}
	
	overlay_viewport_->switchModeByKey(1);
	std::cout << "[UIHandler] Overlay viewport initialized with Normal Mode (key 1)" << std::endl;
	
	initializeSceneObjects();
	
	overlay_viewport_->show(true);
	std::cout << "[UIHandler] Overlay viewport shown (will be repositioned by JavaScript)" << std::endl;
	
	// Create SlotTexture (Layer 2) - Extended horizontally across full width
	// Positioned to show it's not limited by viewport boundaries
	overlay_viewport_->createSlotTexture(0, 50, 1920, 150);
	overlay_viewport_->showSlotTexture(true);
	std::cout << "[UIHandler] SlotTexture created and shown (Layer 2 - extends horizontally across screen)" << std::endl;
		
	SetWindowSubclass(parent_hwnd, ParentWindowProc, 0, reinterpret_cast<DWORD_PTR>(this));
	
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
		
		const int inset = 5;
		overlay_viewport_->setPosition(
			left_panel_width + inset, 
			inset, 
			viewport_width - (2 * inset), 
			viewport_height - (2 * inset)
		);
		
		// Ensure Z-order is maintained after position update
		overlay_viewport_->ensureProperZOrder();
	}
}

static VOID CALLBACK RenderTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) 
{
	UIHandler* handler = reinterpret_cast<UIHandler*>(idEvent);

	if (handler && handler->getOverlay()) 
	{
		CefDoMessageLoopWork();
		
		InvalidateRect(handler->getOverlay()->getHandle(), nullptr, FALSE);
		
		// Also invalidate SlotTexture to ensure it renders
		if (handler->getOverlay()->getSlotTexture()) {
			InvalidateRect(handler->getOverlay()->getSlotTexture()->getHandle(), nullptr, FALSE);
		}
	}
}

void UIHandler::startRenderTimer() 
{
	if (timer_id_ == 0 && overlay_viewport_) 
	{
		// Set high resolution timer for consistent 60 FPS (16.67ms)
		timeBeginPeriod(1);
		timer_id_ = SetTimer(nullptr, reinterpret_cast<UINT_PTR>(this), 16, RenderTimerProc);
	}
}

void UIHandler::stopRenderTimer() 
{
	if (timer_id_ != 0) 
	{
		KillTimer(nullptr, timer_id_);
		timeEndPeriod(1);
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

			if (handler && handler->overlay_viewport_) 
			{
				// Maintain layer-based Z-order after resize
				handler->overlay_viewport_->ensureProperZOrder();
			}
			break;
		
		case WM_ACTIVATE:
		case WM_WINDOWPOSCHANGED:
			if (handler && handler->overlay_viewport_) 
			{
				// Maintain layer-based Z-order after window events
				handler->overlay_viewport_->ensureProperZOrder();
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
	
	GLFWwindow* glfwWindow = glfwGetCurrentContext();
	HGLRC glfwContext = wglGetCurrentContext();
	HDC glfwHDC = wglGetCurrentDC();

	
	overlay_viewport_->makeContextCurrent();
	
	HGLRC overlayContext = wglGetCurrentContext();
	HDC overlayHDC = wglGetCurrentDC();

	
	if (three_d_scene_) 
	{
		three_d_scene_->initizalize();
	}
	
	if (three_d_scene_) 
	{
		for (auto* obj : three_d_scene_->getObjectsRef()) 
		{
			if (obj && obj->getIsMesh()) 
			{
				Mesh* mesh = static_cast<Mesh*>(obj);
				mesh->finalize();
			}
		}
	}
	
	if (three_d_scene_ && main_camera_) 
	{
		overlay_viewport_->setThreeDScene(three_d_scene_);
		std::cout << "[UIHandler] 3D Scene set to overlay viewport - Objects count: " << three_d_scene_->getObjectsRef().size() << std::endl;
		std::cout << "[UIHandler] 3D Scene camera: " << (three_d_scene_->getActiveCamera() ? three_d_scene_->getActiveCamera()->getName() : "NULL") << std::endl;
	}
	
	scene_initialized_ = true;
}

void UIHandler::reinitializeSingleObject(ThreeDObject* obj)
{
	if (!obj)
	{
		return;
	}
	
	if (!overlay_viewport_)
	{
		return;
	}
	
	
	if (obj->getIsMesh())
	{
		Mesh* mesh = static_cast<Mesh*>(obj);
		overlay_viewport_->reinitializeMeshComponents(mesh);
	}

}

void UIHandler::notifySceneChanged()
{
	if (!CefCurrentlyOn(TID_UI))
	{
		CefPostTask(TID_UI, base::BindOnce(&UIHandler::notifySceneChanged, base::Unretained(this)));
		return;
	}
	
	if (!browser_list_.empty())
	{
		CefRefPtr<CefBrowser> browser = browser_list_.front();
		if (browser)
		{
			CefRefPtr<CefFrame> hierarchy_frame = browser->GetFrameByName("hierarchy_inspector");
			if (hierarchy_frame && hierarchy_frame->IsValid())
			{
				CefString script = "if (typeof fetchSceneObjects === 'function') { fetchSceneObjects(); }";
				hierarchy_frame->ExecuteJavaScript(script, hierarchy_frame->GetURL(), 0);
			}
		}
	}
}


// ---------- Keyboard Handler Implementation ---------

bool UIHandler::OnPreKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event,
	CefEventHandle os_event, bool* is_keyboard_shortcut)
{
	if (!overlay_viewport_ || !overlay_viewport_->getHandle())
		return false;
	
	auto& keyManager = SIMILI::Input::KeyManager::getInstance();
	
	if (event.type == KEYEVENT_KEYDOWN || event.type == KEYEVENT_RAWKEYDOWN)
	{
		LPARAM lParam = 1 | (event.native_key_code << 16);
				
		keyManager.handleKeyDown(static_cast<int>(event.windows_key_code), lParam);
		
		// Additionally send mode keys (1-4) to HTML renderer for UI visual feedback
		if (event.windows_key_code >= '1' && event.windows_key_code <= '4')
		{
			HtmlTextureRenderer* html_renderer = overlay_viewport_->getHtmlTextureRenderer();
			if (html_renderer) 
			{
				html_renderer->sendKeyEvent(event);

			}
		}
		
		return true; 
	}
	else if (event.type == KEYEVENT_KEYUP)
	{
		
		keyManager.handleKeyUp(static_cast<int>(event.windows_key_code));
		return true;
	}
	
	return false;
}

bool UIHandler::OnKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event,
CefEventHandle os_event)
{

	return false;
}
