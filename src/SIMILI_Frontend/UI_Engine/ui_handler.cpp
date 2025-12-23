#include "ui_handler.hpp"
#include "viewportLogic/HTMLTextureRenderer/HtmlTextureRenderer.hpp"
#include "viewportLogic/HTMLTextureRenderer/SlotTexture.hpp"
#include "viewportLogic/KeyManagement/KeyManager.hpp"
#include "viewportLogic/Keymanagement/IFrameSizeStocker.hpp"
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
#include <unordered_map>
#include <commctrl.h>  
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <windows.h>
#include <timeapi.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winmm.lib")

// Static map to associate timer IDs with UIHandler instances
static std::unordered_map<UINT_PTR, UIHandler*> g_timerHandlerMap;

UIHandler::UIHandler() : parent_hwnd_(nullptr), timer_id_(0),
	last_viewport_update_time_(0), last_viewport_x_(0), last_viewport_y_(0), 
	last_viewport_width_(0), last_viewport_height_(0), three_d_scene_(nullptr),
	renderer_(nullptr), main_camera_(nullptr), cube_mesh_ptr_(nullptr), scene_initialized_(false),
	render_message_router_(nullptr), 
	iframe_mouse_detector_(nullptr),
	iframe_size_stocker_(&IFrameSizeStocker::getInstance()),
	current_mouse_state_(nullptr),
	above_overlay_state_(nullptr),
	outside_overlay_state_(nullptr),
	last_detected_region_name_("")
{
	// Initialize mouse states
	above_overlay_state_ = new SIMILI::Input::Mouse_Above_Overlay_State();
	outside_overlay_state_ = new SIMILI::Input::Mouse_Outside_Overlay_State();
	
	// Start with null state (will be set on first mouse detection)
	current_mouse_state_ = nullptr;
	
	std::cout << "[UIHandler] Mouse states created (will activate on first mouse detection)" << std::endl;
}

UIHandler::~UIHandler() 
{
	stopRenderTimer();
	
	// Clean up mouse states
	if (above_overlay_state_) 
	{
		delete above_overlay_state_;
		above_overlay_state_ = nullptr;
	}
	if (outside_overlay_state_)
	{
		delete outside_overlay_state_;
		outside_overlay_state_ = nullptr;
	}
	current_mouse_state_ = nullptr;
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
					
					// Force update of panel bounds after viewport resize
					updatePanelBoundsFromStocker();
					
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


// -------- Rendering ------------

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
	}

	overlay_viewport_->switchModeByKey(1);
	
	initializeSceneObjects();
	
	overlay_viewport_->show(true);
	
	enableSlotTextureRendering(false);  
	
	if (iframe_mouse_detector_) {
		iframe_mouse_detector_->setWindowHandle(parent_hwnd);
	}		

	startRenderTimer();
	
	updatePanelBoundsFromStocker();
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
		
		overlay_viewport_->ensureProperZOrder();
	}
}

static VOID CALLBACK RenderTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) 
{
	// Retrieve handler from static map using timer ID
	auto it = g_timerHandlerMap.find(idEvent);
	if (it == g_timerHandlerMap.end()) {
		std::cout << "[RenderTimerProc] ERROR: Handler not found for timer ID " << idEvent << std::endl;
		return;
	}
	
	UIHandler* handler = it->second;


	if (handler && handler->getOverlay()) 
	{
		CefDoMessageLoopWork();
		
		InvalidateRect(handler->getOverlay()->getHandle(), nullptr, FALSE);
		
		if (handler->getOverlay()->getSlotTexture()) 
		{
			InvalidateRect(handler->getOverlay()->getSlotTexture()->getHandle(), nullptr, FALSE);
		}
		
		static int frameCounter = 0;
		frameCounter++;
		
		auto mouseDetector = handler->getIFrameMouseDetector();	

		if (mouseDetector && frameCounter % 30 == 0) 
		{
			POINT screenPt;
			GetCursorPos(&screenPt);
			
			SIMILI::Input::MouseRegion region = mouseDetector->detectMouseRegion(screenPt.x, screenPt.y);
			
			int relativeX, relativeY;
			mouseDetector->getRelativePosition(screenPt.x, screenPt.y, relativeX, relativeY);
			
			std::string regionName = "Unknown";
			switch (region) 
			{
				case SIMILI::Input::MouseRegion::Outside:
					regionName = "Outside";
					break;
				case SIMILI::Input::MouseRegion::HierarchyPanel:
					regionName = "HierarchyPanel";
					break;
				case SIMILI::Input::MouseRegion::ViewportPanel:
					regionName = "ViewportPanel";
					break;
				case SIMILI::Input::MouseRegion::ObjectInspectorPanel:
					regionName = "ObjectInspectorPanel";
					break;
				case SIMILI::Input::MouseRegion::HistoryPanel:
					regionName = "HistoryPanel";
					break;
				case SIMILI::Input::MouseRegion::ProjectViewerPanel:
					regionName = "ProjectViewerPanel";
					break;
				case SIMILI::Input::MouseRegion::Splitter:
					regionName = "Splitter";
					break;
				case SIMILI::Input::MouseRegion::Unknown:
					regionName = "Unknown";
					break;
			}
			
			static std::string lastRegionName = "";
			if (regionName != lastRegionName)
			{
				std::cout << "\n [Mouse] Region: " << regionName << " | Position: (" << relativeX << ", " << relativeY << ")" << std::endl;
				lastRegionName = regionName;
				handler->transitionMouseState(regionName);
			}
		}
	}
}

void UIHandler::startRenderTimer() 
{
	if (timer_id_ == 0 && overlay_viewport_) 
	{
		timeBeginPeriod(1);
		timer_id_ = SetTimer(nullptr, 0, 16, RenderTimerProc);
		
		if (timer_id_ != 0) 
		{
			// Store handler in static map
			g_timerHandlerMap[timer_id_] = this;
		} 
		else 
		{
			std::cerr << "[UIHandler] ERROR: Failed to create timer!" << std::endl;
		}
	}
}

void UIHandler::stopRenderTimer() 
{
	if (timer_id_ != 0) 
	{
		KillTimer(nullptr, timer_id_);
		timeEndPeriod(1);
		
		// Remove from static map
		g_timerHandlerMap.erase(timer_id_);		
		timer_id_ = 0;
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

void UIHandler::enableSlotTextureRendering(bool enable)
{
	if (!overlay_viewport_) return;
	
	if (enable) 
	{
		if (!overlay_viewport_->getSlotTexture()) 
		{
			overlay_viewport_->createSlotTexture(0, 50, 1920, 150);
		} 
		else 
		{
			overlay_viewport_->getSlotTexture()->enableRendering(true);
			std::cout << "[UIHandler] SlotTexture rendering enabled" << std::endl;
		}
	} 
	else 
	{
		if (overlay_viewport_->getSlotTexture()) 
		{
			overlay_viewport_->destroySlotTexture();
			std::cout << "[UIHandler] SlotTexture DESTROYED" << std::endl;
		}
	}
}

// ---------- End of Rendering ----------


// ---------- Scene Update ------------ 

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

// ------------ End of Scene Update ------------- 

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

void UIHandler::updatePanelBoundsFromStocker()
{
	if (!iframe_mouse_detector_ || !iframe_size_stocker_) {
		return;
	}
	
	// Check if window is maximized
	bool isMaximized = false;
	int offsetX = 0;
	int offsetY = 0;
	
	if (parent_hwnd_) {
		WINDOWPLACEMENT placement;
		placement.length = sizeof(WINDOWPLACEMENT);
		
		if (GetWindowPlacement(parent_hwnd_, &placement)) {
			isMaximized = (placement.showCmd == SW_SHOWMAXIMIZED);
			
			if (isMaximized) {
				RECT windowRect, clientRect;
				GetWindowRect(parent_hwnd_, &windowRect);
				GetClientRect(parent_hwnd_, &clientRect);
				
				POINT clientOrigin = {0, 0};
				ClientToScreen(parent_hwnd_, &clientOrigin);
				
				offsetX = clientOrigin.x - windowRect.left;
				offsetY = clientOrigin.y - windowRect.top;
			}
		}
	}
	
	SIMILI::Input::PanelBounds bounds;
	
	auto allFrames = iframe_size_stocker_->getAllIFrames();
	
	// Use  oordinates from JavaScript 
	// Only apply maximized offsets
	for (const auto& pair : allFrames) {
		const std::string& name = pair.first;
		const IFrameData& data = pair.second;
		
		int adjustedX = data.x;
		int adjustedY = data.y;
		
		// Only apply maximized offsets (no DPI scaling)at all)
		if (isMaximized) {
			adjustedX += offsetX;
			adjustedY += offsetY;
		}
		
		if (name == "hierarchy_inspector") 
		{
			bounds.hierarchy.x = adjustedX;
			bounds.hierarchy.y = adjustedY;
			bounds.hierarchy.width = data.width;
			bounds.hierarchy.height = data.height;
		}
		else if (name == "viewport_docking") 
		{
			bounds.viewport.x = adjustedX;
			bounds.viewport.y = adjustedY;
			bounds.viewport.width = data.width;
			bounds.viewport.height = data.height;
		}
		else if (name == "object_inspector") 
		{
			bounds.objectInspector.x = adjustedX;
			bounds.objectInspector.y = adjustedY;
			bounds.objectInspector.width = data.width;
			bounds.objectInspector.height = data.height;
		}
		else if (name == "history_logger") 
		{
			bounds.history.x = adjustedX;
			bounds.history.y = adjustedY;
			bounds.history.width = data.width;
			bounds.history.height = data.height;
		}
		else if (name == "project_viewer") 
		{
			bounds.projectViewer.x = adjustedX;
			bounds.projectViewer.y = adjustedY;
			bounds.projectViewer.width = data.width;
			bounds.projectViewer.height = data.height;
		}
	}
	
	iframe_mouse_detector_->updatePanelBounds(bounds);
	
	std::cout << "[UIHandler] Panel bounds updated - Viewport at (" 
			  << bounds.viewport.x << "," << bounds.viewport.y 
			  << ") size " << bounds.viewport.width << "x" << bounds.viewport.height 
			  << " (Maximized:" << (isMaximized ? "YES" : "NO") 
			  << ", Offsets: X=" << offsetX << ", Y=" << offsetY << ")" << std::endl;
}

void UIHandler::setIFrameMouseDetector(SIMILI::Input::IFrameMouseDetector* detector)
{
	iframe_mouse_detector_ = detector;
}

void UIHandler::logIFrameSizes()
{
	if (!iframe_size_stocker_) {
		return;
	}
	
	updatePanelBoundsFromStocker();
}

void UIHandler::transitionMouseState(const std::string& regionName)
{
	
	last_detected_region_name_ = regionName;
	
	SIMILI::Input::Mouse_State* newState = nullptr;
	
	if (regionName == "ViewportPanel")
	{
		newState = above_overlay_state_;
	}
	else
	{
		newState = outside_overlay_state_;
	}
	
	if (newState != current_mouse_state_)
	{
		if (current_mouse_state_)
		{
			current_mouse_state_->onExit();
		}
		
		current_mouse_state_ = newState;
		
		if (current_mouse_state_)
		{
			current_mouse_state_->onEnter();
		}
	}
}