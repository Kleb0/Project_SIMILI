#include "ui_handler.hpp"
#include "simple_window_delegate.hpp"
#include "viewportLogic/HTMLTextureRenderer/HtmlTextureRenderer.hpp"
#include "viewportLogic/HTMLTextureRenderer/Overlay_HTML_Texture_Renderer.hpp"
#include "viewportLogic/KeyManagement/KeyManager.hpp"
#include "viewportLogic/Keymanagement/MouseControlToOverlay.hpp"
#include "../../Engine/OpenGLScene/ThreeDScene.hpp"
#include "../../Engine/OpenGLScene/OpenGLContext.hpp"
#include "../../WorldObjects/Camera/Camera.hpp"
#include "../../WorldObjects/Mesh/Mesh.hpp"
#include "../../Engine/PrimitivesCreation/CreatePrimitive.hpp"
#include "../../Engine/ThreeDModes/ThreeDMode.hpp"
#include "../../Engine/ThreeDModes/Normal_Mode.hpp"
#include "../../Engine/ThreeDModes/Vertice_Mode.hpp"
#include "../../Engine/ThreeDModes/Face_Mode.hpp"
#include "../../Engine/ThreeDModes/Edge_Mode.hpp"
#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include <iostream>
#include <unordered_map>
#include <commctrl.h>  
#include <glm/glm.hpp>
#include <SDL3/SDL.h>

#pragma comment(lib, "comctl32.lib")

static std::unordered_map<SDL_TimerID, UIHandler*> g_timerHandlerMap;

static UIHandler* g_activeHandler = nullptr;

UIHandler* UIHandler::s_instance_ = nullptr;

UIHandler::UIHandler() : parent_window_(nullptr), timer_id_(0),
	last_viewport_update_time_(0), last_viewport_x_(0), last_viewport_y_(0), 
	last_viewport_width_(0), last_viewport_height_(0), three_d_scene_(nullptr),
	renderer_(nullptr), main_camera_(nullptr), cube_mesh_ptr_(nullptr), scene_initialized_(false),
	render_message_router_(nullptr), 
	iframe_mouse_detector_(nullptr),
	frame_datas_(nullptr),
	window_delegate_(nullptr),
	current_mouse_state_(nullptr),
	above_overlay_state_(nullptr),
	outside_overlay_state_(nullptr),
	above_ui_panel_state_(nullptr),
	last_detected_region_name_(""),
	mouse_control_to_overlay_(nullptr),
	is_camera_operation_locked_(false),
	slot_texture_renderer_(nullptr),
	composite_test_renderer_(nullptr),
	d3d11_device_(nullptr),
	d3d11_device_context_(nullptr),
	dxgi_device_(nullptr),
	d2d_factory_(nullptr),
	d2d_device_(nullptr)
{
	above_overlay_state_ = new SIMILI::Input::Mouse_Above_Overlay_State();
	outside_overlay_state_ = new SIMILI::Input::Mouse_Outside_Overlay_State();
	above_ui_panel_state_ = new SIMILI::Input::Mouse_Above_UI_Panel_State();	
	mouse_control_to_overlay_ = new SIMILI::Input::MouseControlToOverlay();
	
	current_mouse_state_ = nullptr;

	HRESULT hr = S_OK;
	D2D1_FACTORY_OPTIONS options = {};
	options.debugLevel = D2D1_DEBUG_LEVEL_NONE;
	hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &options, (void**)&d2d_factory_);
	
	if (SUCCEEDED(hr))
	{
		UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
		D3D_FEATURE_LEVEL featureLevels[] = {
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
			D3D_FEATURE_LEVEL_9_3,
			D3D_FEATURE_LEVEL_9_2,
			D3D_FEATURE_LEVEL_9_1
		};
		D3D_FEATURE_LEVEL featureLevel;
		hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			0,
			creationFlags,
			featureLevels,
			ARRAYSIZE(featureLevels),
			D3D11_SDK_VERSION,
			&d3d11_device_,
			&featureLevel,
			&d3d11_device_context_
		);
	}
	
	if (SUCCEEDED(hr))
	{
		hr = d3d11_device_->QueryInterface(__uuidof(IDXGIDevice1), (void**)&dxgi_device_);
	}
	
	if (SUCCEEDED(hr) && d2d_factory_)
	{
		hr = d2d_factory_->CreateDevice(dxgi_device_, &d2d_device_);
	}
	
	s_instance_ = this;
}

UIHandler::~UIHandler() 
{
	if (timer_id_ != 0) 
	{
		SDL_RemoveTimer(timer_id_);
		g_timerHandlerMap.erase(timer_id_);		
		
		if (g_activeHandler == this)
		{
			g_activeHandler = nullptr;
		}
		
		timer_id_ = 0;
	}
	
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
	if (above_ui_panel_state_)
	{
		delete above_ui_panel_state_;
		above_ui_panel_state_ = nullptr;
	}
	if (mouse_control_to_overlay_)
	{
		delete mouse_control_to_overlay_;
		mouse_control_to_overlay_ = nullptr;
	}
	if (frame_datas_)
	{
		delete frame_datas_;
		frame_datas_ = nullptr;
	}
	if (slot_texture_renderer_)
	{
		delete slot_texture_renderer_;
		slot_texture_renderer_ = nullptr;
	}
	if (composite_test_renderer_)
	{
		delete composite_test_renderer_;
		composite_test_renderer_ = nullptr;
	}
	current_mouse_state_ = nullptr;

	if (d2d_device_)
	{
		d2d_device_->Release();
		d2d_device_ = nullptr;
	}
	if (dxgi_device_)
	{
		dxgi_device_->Release();
		dxgi_device_ = nullptr;
	}
	if (d3d11_device_context_)
	{
		d3d11_device_context_->Release();
		d3d11_device_context_ = nullptr;
	}
	if (d3d11_device_)
	{
		d3d11_device_->Release();
		d3d11_device_ = nullptr;
	}
	if (d2d_factory_)
	{
		d2d_factory_->Release();
		d2d_factory_ = nullptr;
	}
	
	if (s_instance_ == this)
	{
		s_instance_ = nullptr;
	}
}

CefRefPtr<CefBrowserProcessHandler> UIHandler::GetBrowserProcessHandler() 
{
	return this;
}

CefRefPtr<CefRenderProcessHandler> UIHandler::GetRenderProcessHandler() 
{
	return this;
}

UIHandler* UIHandler::getInstance()
{
	return s_instance_;
}

void UIHandler::setInstance(UIHandler* handler)
{
	s_instance_ = handler;
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
		
			if (overlay_viewport_ && parent_window_ && browser) 
			{
				int final_x = static_cast<int>(js_x * dpiScale);
				int final_y = static_cast<int>(js_y * dpiScale);
				int final_width = static_cast<int>(width * dpiScale);
				int final_height = static_cast<int>(height * dpiScale);
				
				Uint64 current_time = SDL_GetTicks();
				bool position_changed = (abs(final_x - last_viewport_x_) > 2 || 
										abs(final_y - last_viewport_y_) > 2 ||
										abs(final_width - last_viewport_width_) > 2 ||
										abs(final_height - last_viewport_height_) > 2);
				bool time_elapsed = (current_time - last_viewport_update_time_) > 16;
				
				if (position_changed || time_elapsed || !overlay_viewport_->isVisible()) 
				{
					overlay_viewport_->setPosition(final_x, final_y, final_width, final_height);
					
					if (mouse_control_to_overlay_)
					{
						mouse_control_to_overlay_->resetMousePosition();
					}
					
					if (three_d_scene_ && three_d_scene_->getActiveCamera())
					{
						three_d_scene_->getActiveCamera()->setResolution(final_width, final_height, dpiScale);
					}
					
					last_viewport_update_time_ = current_time;
					last_viewport_x_ = final_x;
					last_viewport_y_ = final_y;
					last_viewport_width_ = final_width;
					last_viewport_height_ = final_height;
					
					captureIFramePositions();
					
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

void UIHandler::createOverlayViewport(SDL_Window* parent_window) 
{
	parent_window_ = parent_window;
		
	if (!overlay_viewport_) 
	{
		overlay_viewport_ = std::make_unique<OverlayViewport>();
	}

	int window_width, window_height;
	SDL_GetWindowSize(parent_window, &window_width, &window_height);

	int overlay_x = 0;
	int overlay_y = 0;
	int overlay_w = 100;
	int overlay_h = 100;
	
	overlay_viewport_->create(parent_window, overlay_x, overlay_y, overlay_w, overlay_h);
	
	overlay_viewport_->setUIHandler(this);
	
	if (three_d_scene_) 
	{
		overlay_viewport_->setThreeDScene(three_d_scene_);
	}

	overlay_viewport_->switchModeByKey(1);
	
	initializeSceneObjects();
	
	overlay_viewport_->show(true);
	
	enableSlotTextureRendering(false);
	enableCompositeTestRenderer(true);
	
	if (iframe_mouse_detector_) {
		iframe_mouse_detector_->setWindowHandle(parent_window);
	}		

	if (timer_id_ == 0 && overlay_viewport_) 
	{
		timer_id_ = SDL_AddTimer(16, RenderTimerProc, this);
		
		if (timer_id_ != 0) 
		{
			g_timerHandlerMap[timer_id_] = this;
			g_activeHandler = this;
			std::cout << "[UIHandler] Render timer started with ID " << timer_id_ << std::endl;
		}
	}
	
	updatePanelBoundsFromStocker();
}

void UIHandler::updateOverlayPosition() 
{
	if (overlay_viewport_ && parent_window_) 
	{
		int window_width, window_height;
		SDL_GetWindowSize(parent_window_, &window_width, &window_height);
		
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
	}
}

static Uint32 SDLCALL RenderTimerProc(void* param, SDL_TimerID timerID, Uint32 interval) 
{
	auto it = g_timerHandlerMap.find(timerID);
	if (it == g_timerHandlerMap.end()) 
	{
		return 0;
	}
	
	UIHandler* handler = it->second;


	if (handler && handler->getOverlay()) 
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_EVENT_QUIT)
			{
				CefQuitMessageLoop();
				return 0;
			}
		}
		
		if (auto windowDelegate = handler->getWindowDelegate())
		{
			windowDelegate->pollWindowEvents();
		}
		
		static int frameCounter = 0;
		frameCounter++;		
		
		// Update mouse position tracking in MouseControlToOverlay
		if (handler->getMouseControlToOverlay())
		{
			handler->getMouseControlToOverlay()->updateMousePosition();
			
			// Update wheel state to handle timeout
			handler->getMouseControlToOverlay()->updateWheelState();
		}
		
		auto mouseDetector = handler->getIFrameMouseDetector();	

		if (mouseDetector) 
		{
			// Get current mouse position in screen coordinates (global)
			float mouseX, mouseY;
			SDL_GetGlobalMouseState(&mouseX, &mouseY);
			
			// Detect the region at every frame for state management
			auto region = mouseDetector->detectMouseRegion(static_cast<int>(mouseX), static_cast<int>(mouseY));
			
			// Convert enum to readable string
			std::string regionName;
			switch (region)
			{
				case SIMILI::Input::MouseRegion::Outside:
					regionName = "Outside Window";
					break;
				case SIMILI::Input::MouseRegion::HierarchyPanel:
					regionName = "Hierarchy Panel";
					break;
				case SIMILI::Input::MouseRegion::ViewportPanel:
					regionName = "Viewport Panel";
					break;
				case SIMILI::Input::MouseRegion::ObjectInspectorPanel:
					regionName = "Object Inspector Panel";
					break;
				case SIMILI::Input::MouseRegion::HistoryPanel:
					regionName = "History Panel";
					break;
				case SIMILI::Input::MouseRegion::ProjectViewerPanel:
					regionName = "Project Viewer Panel";
					break;
				case SIMILI::Input::MouseRegion::PanelAboveUI:
					regionName = "Panel Above UI";
					break;
				case SIMILI::Input::MouseRegion::Splitter:
					regionName = "Splitter";
					break;
				case SIMILI::Input::MouseRegion::Unknown:
				default:
					regionName = "Unknown";
					break;
			}
			
			if (regionName != handler->getLastDetectedRegionName())
			{		
				// Only allow state transitions if camera is not being panned
				if (!handler->isCameraOperationLocked())
				{
					handler->transitionMouseState(regionName);
				}
			}
		}
		
		// Camera lateral movement (panning)
		if (handler->getMouseControlToOverlay()->isShiftLeftClickActive() && handler->getCurrentMouseState() == handler->getAboveOverlayState())
		{
			// Lock state transitions during camera panning
			handler->setCameraOperationLocked(true);
			
			if (handler->getOverlay())
			{
				int deltaX = handler->getMouseControlToOverlay()->getMouseDeltaX();
				int deltaY = handler->getMouseControlToOverlay()->getMouseDeltaY();
				handler->getOverlay()->MoveCameraLaterally(deltaX, deltaY);
			}
		}
		else
		{
			// Unlock state transitions when camera panning stops
			if (handler->isCameraOperationLocked())
			{
				handler->setCameraOperationLocked(false);
			}
		}
		
		if (handler->getCurrentMouseState() == handler->getAboveOverlayState())
		{
			if (handler->getMouseControlToOverlay()->hasClickEvent())
			{
				if (handler->getOverlay())
				{
					int mouseX = handler->getMouseControlToOverlay()->getCurrentMouseX();
					int mouseY = handler->getMouseControlToOverlay()->getCurrentMouseY();
					handler->getOverlay()->shootRaycastFromUIHandler(mouseX, mouseY);
				}
			}
		}
		
		// Check Shift key state using SDL3
		const bool* keystate = SDL_GetKeyboardState(NULL);
		bool isShiftPressed = (keystate[SDL_SCANCODE_LSHIFT] || keystate[SDL_SCANCODE_RSHIFT]);

		if (handler->getMouseControlToOverlay()->isLeftButtonClicking() && !isShiftPressed && handler->getCurrentMouseState() == handler->getAboveOverlayState())
		{
			if (handler->getMouseControlToOverlay()->isClickHeldForDuration(200))
			{
				if (handler->getOverlay() && !handler->getOverlay()->isGizmoActive())
				{
					int deltaX = handler->getMouseControlToOverlay()->getMouseDeltaX();
					int deltaY = handler->getMouseControlToOverlay()->getMouseDeltaY();
					handler->getOverlay()->ProcessCameraOrbiting(deltaX, deltaY);
				}
			}
		}
		
		// Process mouse wheel input ONLY when mouse is above overlay
		if (handler->getCurrentMouseState() == handler->getAboveOverlayState())
		{
			if (handler->getMouseControlToOverlay()->hasWheelInput())
			{
				int wheelDirection = handler->getMouseControlToOverlay()->getMouseWheelDirection();
				handler->getOverlay()->ProcessZoom(wheelDirection);
			}
		}
		else
		{
			// If mouse is not above overlay, clear any pending wheel input
			if (handler->getMouseControlToOverlay()->hasWheelInput())
			{
				handler->getMouseControlToOverlay()->resetWheelDirection();
			}
		}
		
		if (handler->getOverlay() && handler->getMouseControlToOverlay())
		{
			SDL_Window* overlayWindow = handler->getOverlay()->getHandle();

			if (overlayWindow)
			{
				// Get mouse position relative to the overlay window
				float mouseX, mouseY;
				Uint32 mouseState = SDL_GetMouseState(&mouseX, &mouseY);
				
				bool leftDown = handler->getMouseControlToOverlay()->isLeftButtonClicking();
				bool rightDown = (mouseState & SDL_BUTTON_RMASK) != 0;
				bool middleDown = (mouseState & SDL_BUTTON_MMASK) != 0;
					
					float wheelDelta = 0.0f;

					if (handler->getMouseControlToOverlay()->hasWheelInput())
					{
						wheelDelta = static_cast<float>(handler->getMouseControlToOverlay()->getMouseWheelDirection());
					}
					
				handler->getOverlay()->injectMouseInputs(
					static_cast<int>(mouseX),
					static_cast<int>(mouseY),
					leftDown,
					rightDown,
					middleDown,
					wheelDelta
				);
				
				handler->getOverlay()->render();
			}
		}
		
		if (handler->getSlotTextureRenderer() && handler->getSlotTextureRenderer()->isRenderingEnabled())
		{
			handler->getSlotTextureRenderer()->render();
		}
		
		if (handler->getCompositeTestRenderer() && handler->getCompositeTestRenderer()->isRenderingEnabled())
		{
			handler->getCompositeTestRenderer()->render();
		}
	}
	
	return interval; // Continue timer with same interval
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
	if (!parent_window_) return;
	
	if (enable) 
	{
		if (!slot_texture_renderer_) 
		{
			SDL_GLContext shareContext = nullptr;
			if (overlay_viewport_)
			{
				overlay_viewport_->makeContextCurrent();
				shareContext = overlay_viewport_->getGLContext();
			}
			
			slot_texture_renderer_ = new Overlay_HTML_Texture_Renderer("file:///ui/hello_cef.html");
			slot_texture_renderer_->create(parent_window_, 0, 50, 1920, 150, shareContext);
			slot_texture_renderer_->enableRendering(true);
			slot_texture_renderer_->show(true);
			std::cout << "[UIHandler] Overlay_HTML_Texture_Renderer created and shown" << std::endl;
		} 
		else 
		{
			slot_texture_renderer_->enableRendering(true);
			slot_texture_renderer_->show(true);

			// Window refresh is handled automatically by SDL3

			std::cout << "[UIHandler] Overlay_HTML_Texture_Renderer rendering enabled" << std::endl;
		}
	} 
	else 
	{
		if (slot_texture_renderer_) 
		{
			slot_texture_renderer_->enableRendering(false);
			slot_texture_renderer_->show(false);
			
			auto* temp = slot_texture_renderer_;
			slot_texture_renderer_ = nullptr;
			
			temp->destroy();
			
			std::cout << "[UIHandler] Overlay_HTML_Texture_Renderer DESTROYED" << std::endl;
		}
	}
}

void UIHandler::enableCompositeTestRenderer(bool enable)
{
	if (!parent_window_) return;
	
	if (enable)
	{
		composite_test_renderer_ = new Overlay_HTML_Texture_Renderer("file:///ui/Composite_Test.html");

		composite_test_renderer_->setSharedDevices(d3d11_device_, dxgi_device_, d2d_factory_, d2d_device_);
		composite_test_renderer_->enableDirectComposition(true);
		composite_test_renderer_->SetParentByName("viewport_panel");
		// composite_test_renderer_->FilterColor(0, 0, 250);
		composite_test_renderer_->EnableTransparency(0.8f);
		composite_test_renderer_->changeScaleByValue(0.5f);
		composite_test_renderer_->maximise();
		composite_test_renderer_->enableBrowserClassicEvent(true);
		composite_test_renderer_->create(parent_window_, 0, 0, 500, 500, nullptr);
		composite_test_renderer_->enableRendering(true);
		composite_test_renderer_->show(true);
		
		composite_test_renderer_->RequestPanelPositionUpdate(PanelAnchorPosition::CurrentAnchorState);
		captureIFramePositions();

	}
	else
	{
		if (composite_test_renderer_)
		{
			composite_test_renderer_->enableRendering(false);
			composite_test_renderer_->show(false);
			
			auto* temp = composite_test_renderer_;
			composite_test_renderer_ = nullptr;
			
			temp->destroy();
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
	
	// Make overlay context current for scene initialization
	overlay_viewport_->makeContextCurrent();
	
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

void UIHandler::initializeFrameDatas(SimpleWindowDelegate* windowDelegate)
{
	if (frame_datas_)
	{
		delete frame_datas_;
		frame_datas_ = nullptr;
	}
	
	frame_datas_ = new SIMILI::Frontend::FrameDatas(windowDelegate);
	std::cout << "[UIHandler] FrameDatas initialized with SimpleWindowDelegate" << std::endl;
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
	if (!iframe_mouse_detector_ || !window_delegate_) {
		return;
	}
	
	SIMILI::Input::PanelBounds bounds;
	
	auto allFrames = window_delegate_->getAllIFrames();
	
	for (const auto& pair : allFrames) {
		const std::string& name = pair.first;
		const IFrameData& data = pair.second;
		
		if (name == "hierarchy_panel") 
		{
			bounds.hierarchy.x = data.x;
			bounds.hierarchy.y = data.y;
			bounds.hierarchy.width = data.width;
			bounds.hierarchy.height = data.height;
		}
		else if (name == "viewport_panel") 
		{
			bounds.viewport.x = data.x;
			bounds.viewport.y = data.y;
			bounds.viewport.width = data.width;
			bounds.viewport.height = data.height;
		}
		else if (name == "object_inspector_panel") 
		{
			bounds.objectInspector.x = data.x;
			bounds.objectInspector.y = data.y;
			bounds.objectInspector.width = data.width;
			bounds.objectInspector.height = data.height;
		}
		else if (name == "history_panel") 
		{
			bounds.history.x = data.x;
			bounds.history.y = data.y;
			bounds.history.width = data.width;
			bounds.history.height = data.height;
		}
		else if (name == "project_viewer_panel") 
		{
			bounds.projectViewer.x = data.x;
			bounds.projectViewer.y = data.y;
			bounds.projectViewer.width = data.width;
			bounds.projectViewer.height = data.height;
		}
	}
	
	iframe_mouse_detector_->updatePanelBounds(bounds);
	
	std::cout << "[UIHandler] Panel bounds updated - Viewport at (" 
			  << bounds.viewport.x << "," << bounds.viewport.y 
			  << ") size " << bounds.viewport.width << "x" << bounds.viewport.height << std::endl;
}

void UIHandler::setIFrameMouseDetector(SIMILI::Input::IFrameMouseDetector* detector)
{
	iframe_mouse_detector_ = detector;
}

void UIHandler::logIFrameSizes()
{
	if (!window_delegate_) 
	{
		return;
	}
	
	updatePanelBoundsFromStocker();
}

void UIHandler::captureIFramePositions()
{
	if (!frame_datas_ || !parent_window_)
	{
		std::cout << "[UIHandler] Cannot capture iframe positions - invalid state" << std::endl;
		return;
	}
	
	frame_datas_->captureAllFrames(parent_window_);
	
	if (composite_test_renderer_ && composite_test_renderer_->isActive())
	{
		SIMILI::Frontend::IFrameScreenData panelData;
		if (frame_datas_->getFrameData("panel_above_UI", panelData))
		{
			
			int screenX = composite_test_renderer_->getScreenX();
			int screenY = composite_test_renderer_->getScreenY();
			int width = composite_test_renderer_->getWidth();
			int height = composite_test_renderer_->getHeight();
			
		// Get window position to convert screen coords to window-relative coords
		int windowX, windowY;
		SDL_GetWindowPosition(parent_window_, &windowX, &windowY);
		int topLeftX = screenX - windowX;
		int topLeftY = screenY - windowY;
		
		// Get DPI scale using SDL3
		float dpiScale = 1.0f;
		SDL_DisplayID displayID = SDL_GetDisplayForWindow(parent_window_);
		if (displayID != 0)
		{
			dpiScale = SDL_GetDisplayContentScale(displayID);
		}
		
		// Convert physical pixels to CSS pixels for coordinates AND dimensions
		int relativeX = static_cast<int>(topLeftX / dpiScale);
		int relativeY = static_cast<int>(topLeftY / dpiScale);
		int cssWidth = static_cast<int>(width / dpiScale);
		int cssHeight = static_cast<int>(height / dpiScale);
		
		frame_datas_->updateFrameData("panel_above_UI", relativeX, relativeY, cssWidth, cssHeight, relativeX, relativeY, parent_window_);
		
		std::cout <<"\n [UIHandler] -------------- Test Panel Above UI Frame Data --------------" << std::endl;
		std::cout << "[UIHandler] panel_above_UI updated - RelativeX: " << relativeX 
				  << ", RelativeY: " << relativeY 
				  << ", Width (CSS): " << cssWidth 
				  << ", Height (CSS): " << cssHeight 
				  << " (Physical: " << width << "x" << height << ", DPI: " << dpiScale << ")" << std::endl;
		std::cout << "-------------------------------------------------------------\n" << std::endl;
		}
	}
	
	updateIFrameMouseDetectorFromFrameDatas();
	
	if (overlay_viewport_ && three_d_scene_ && three_d_scene_->getActiveCamera())
	{
		SIMILI::Frontend::IFrameScreenData ViewportPanelSize;

		if (frame_datas_->getFrameData("viewport_panel", ViewportPanelSize))
		{
			float dpiScale = ViewportPanelSize.dpiScale;
			int Width = static_cast<int>(ViewportPanelSize.width * dpiScale);
			int Height = static_cast<int>(ViewportPanelSize.height * dpiScale);
			
			Camera* cam = three_d_scene_->getActiveCamera();

			cam->setResolution(Width, Height, dpiScale);

			std::cout << "[UIHandler] Camera resolution updated: " << "x : " << ViewportPanelSize.height << " y : " << ViewportPanelSize.width
			<< " DPI scale: " << dpiScale  << std::endl;
			
			overlay_viewport_->updateViewportDimensions(Width, Height);

			// Handle panel repositioning if requested
			if (composite_test_renderer_ && composite_test_renderer_->needs_repositioning())
			{
				composite_test_renderer_->UpdateParentData(
					ViewportPanelSize.clientX,
					ViewportPanelSize.clientY,
					ViewportPanelSize.width,
					ViewportPanelSize.height,
					dpiScale
				);

				// Apply the requested anchor position
				switch (composite_test_renderer_->anchor_position())
				{
					case PanelAnchorPosition::Centerize:
						composite_test_renderer_->setCenterize();
						break;
					case PanelAnchorPosition::TopLeft:
						composite_test_renderer_->setTopLeft();
						break;
					case PanelAnchorPosition::TopRight:
						composite_test_renderer_->setTopRight();
						break;
					case PanelAnchorPosition::BottomLeft:
						composite_test_renderer_->setBottomLeft();
						break;
					case PanelAnchorPosition::BottomRight:
						composite_test_renderer_->setBottomRight();
						break;
					case PanelAnchorPosition::MiddleLeft:
						composite_test_renderer_->setMiddleLeft();
						break;
					case PanelAnchorPosition::MiddleRight:
						composite_test_renderer_->setMiddleRight();
						break;
				}

				composite_test_renderer_->clearRepositioningFlag();

				int newScreenX = composite_test_renderer_->getScreenX();
				int newScreenY = composite_test_renderer_->getScreenY();
				int newWidth = composite_test_renderer_->getWidth();
				int newHeight = composite_test_renderer_->getHeight();
				
				// Get window position to convert screen coords to window-relative coords
				int newWindowX, newWindowY;
				SDL_GetWindowPosition(parent_window_, &newWindowX, &newWindowY);
				int newTopLeftX = newScreenX - newWindowX;
				int newTopLeftY = newScreenY - newWindowY;
				
				// Convert physical pixels to CSS pixels for coordinates AND dimensions
				int newRelativeX = static_cast<int>(newTopLeftX / dpiScale);
				int newRelativeY = static_cast<int>(newTopLeftY / dpiScale);
				int newCssWidth = static_cast<int>(newWidth / dpiScale);
				int newCssHeight = static_cast<int>(newHeight / dpiScale);
				
				frame_datas_->updateFrameData("panel_above_UI", newRelativeX, newRelativeY, newCssWidth, newCssHeight, newRelativeX, newRelativeY, parent_window_);
								
				// Update IFrameMouseDetector with new coordinates
				updateIFrameMouseDetectorFromFrameDatas();
			}

		}
	}
}

void UIHandler::updateIFrameMouseDetectorFromFrameDatas()
{
	if (!frame_datas_ || !iframe_mouse_detector_)
	{
		return;
	}
	
	std::map<std::string, SIMILI::Input::IFrameScreenDataSimple> simpleFrameData;
	
	const auto& frameDataMap = frame_datas_->getFrameData();

	for (const auto& pair : frameDataMap)
	{
		const SIMILI::Frontend::IFrameScreenData& data = pair.second;
		SIMILI::Input::IFrameScreenDataSimple simple;
		
		simple.name = data.name;
		
		if (data.name == "panel_above_UI" && composite_test_renderer_ && composite_test_renderer_->isActive())
		{
			int screenX = composite_test_renderer_->getScreenX();
			int screenY = composite_test_renderer_->getScreenY();
			int width = composite_test_renderer_->getWidth();
			int height = composite_test_renderer_->getHeight();
			
			// Get window position to convert screen coords to window-relative coords
			int topLeftX = screenX;
			int topLeftY = screenY;
			if (parent_window_)
			{
				int windowX, windowY;
				SDL_GetWindowPosition(parent_window_, &windowX, &windowY);
				topLeftX = screenX - windowX;
				topLeftY = screenY - windowY;
			}
			
			// Get DPI scale using SDL3
			float dpiScale = 1.0f;
			if (parent_window_)
			{
				SDL_DisplayID displayID = SDL_GetDisplayForWindow(parent_window_);
				if (displayID != 0)
				{
					dpiScale = SDL_GetDisplayContentScale(displayID);
				}
			}
			
			simple.clientX = static_cast<int>(topLeftX / dpiScale);
			simple.clientY = static_cast<int>(topLeftY / dpiScale);
			simple.width = static_cast<int>(width / dpiScale);
			simple.height = static_cast<int>(height / dpiScale);
		}
		else
		{
			simple.clientX = data.clientX;
			simple.clientY = data.clientY;
			simple.width = data.width;
			simple.height = data.height;
		}
		
		simpleFrameData[data.name] = simple;
	}
	
	iframe_mouse_detector_->updatePanelBoundsFromFrameData(simpleFrameData);
	
}

void UIHandler::transitionMouseState(const std::string& regionName)
{
	
	last_detected_region_name_ = regionName;
	
	SIMILI::Input::Mouse_State* newState = nullptr;
	
	if (regionName == "Viewport Panel")
	{
		newState = above_overlay_state_;
	}
	else if (regionName == "Panel Above UI")
	{
		newState = above_ui_panel_state_;
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
		
		if (mouse_control_to_overlay_)
		{
			mouse_control_to_overlay_->setMouseState(current_mouse_state_);
		}
	}
}

void UIHandler::CallTestFromServer()
{
	
	if (!CefCurrentlyOn(TID_UI))
	{
		// the texture need to be rendered on the UI thread, so we post a task to enable it there
		CefPostTask(TID_UI, base::BindOnce(&UIHandler::enableSlotTextureRendering, base::Unretained(this), true));
		return;
	}
	
	enableSlotTextureRendering(true);

}