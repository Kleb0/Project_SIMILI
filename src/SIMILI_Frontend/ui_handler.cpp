#include "ui_handler.hpp"
#include "viewportLogic/HTMLTextureRenderer/HtmlTextureRenderer.hpp"
#include "viewportLogic/HTMLTextureRenderer/Overlay_HTML_Texture_Renderer.hpp"
#include "viewportLogic/KeyManagement/KeyManager.hpp"
#include "viewportLogic/Keymanagement/MouseController.hpp"
#include "../../Engine/VulkanScene/VKScene.Hpp"
#include "../../Engine/VulkanScene/VKcontext.hpp"
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

UIHandler::UIHandler() : parent_sdl_window_(nullptr), parent_window_(nullptr), window_handle_(nullptr), timer_id_(0),
	last_viewport_update_time_(0), last_viewport_x_(0), last_viewport_y_(0), 
	last_viewport_width_(0), last_viewport_height_(0), vk_scene_(nullptr),
	vk_renderer_(nullptr), main_camera_(nullptr), cube_mesh_ptr_(nullptr), scene_initialized_(false),
	render_message_router_(nullptr), 
	frame_datas_(nullptr),
	window_delegate_(nullptr),
	mouse_controller_(nullptr),
	is_camera_operation_locked_(false),
	slot_texture_renderer_(nullptr),
	composite_test_renderer_(nullptr),
	cef_drawer_(nullptr),
	d3d11_device_(nullptr),
	d3d11_device_context_(nullptr),
	dxgi_device_(nullptr),
	d2d_factory_(nullptr),
	d2d_device_(nullptr),
	overlay_viewport_(nullptr)
{
	mouse_controller_ = new SIMILI::Input::MouseController();

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

void UIHandler::startRenderTimer()
{
	if (timer_id_ == 0)
	{
		auto timerCallback = [](void* param, SDL_TimerID timerID, Uint32 interval) -> Uint32
		{
			UIHandler* handler = static_cast<UIHandler*>(param);
			if (handler)
			{
				return RenderTimerProc(param, timerID, interval, handler->parent_sdl_window_, handler->getCEFDrawer());
			}
			return 0;
		};
		
		timer_id_ = SDL_AddTimer(16, timerCallback, this);
		
		if (timer_id_ != 0)
		{
			g_timerHandlerMap[timer_id_] = this;
			g_activeHandler = this;
			std::cout << "[UIHandler] Render timer started (ID: " << timer_id_ << ")" << std::endl;
		}
		else
		{
			std::cerr << "[UIHandler] Failed to create render timer" << std::endl;
		}
	}
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
	
	if (mouse_controller_)
	{
		delete mouse_controller_;
		mouse_controller_ = nullptr;
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
	if (cef_drawer_)
	{
		cef_drawer_ = nullptr; // CEF will release it via reference counting
	}

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

void UIHandler::set_Overlay_Viewport(OverlayViewport* viewport)
{
	overlay_viewport_ = viewport;
}

void UIHandler::Set_SDLParent(SDL_ApplicationWindow* parentWindow)
{
	parent_sdl_window_ = parentWindow;
	if (parentWindow)
	{
		parent_window_ = parentWindow->getHandle();
		if (mouse_controller_)
		{
			mouse_controller_->setWindowHandle(parent_window_);
		}
	}
}

void UIHandler::Set_DOM(CEF_Drawer* drawer)
{
	cef_drawer_ = drawer;
}

void UIHandler::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) 
{
	if (!render_message_router_) 
	{
		CefMessageRouterConfig config;
		render_message_router_ = CefMessageRouterRendererSide::Create(config);
	}
	
	if (render_message_router_) 
	{
		render_message_router_->OnContextCreated(browser, frame, context);
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
CefRefPtr<CefRenderHandler> UIHandler::GetRenderHandler()
{
	return cef_drawer_;
}


void UIHandler::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) 
{
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

// ------------------- Render Timer Callback -------------------


static Uint32 SDLCALL RenderTimerProc(void* param, SDL_TimerID timerID, Uint32 interval, SDL_ApplicationWindow* parentWindow, CEF_Drawer* dom) 
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
	
		if (handler->getMouseController())
		{
			handler->getMouseController()->updateMousePosition();
			handler->getMouseController()->updateWheelState();

			int mouseX = handler->getMouseController()->getCurrentMouseClientX();
			int mouseY = handler->getMouseController()->getCurrentMouseClientY();
			std::string regionName = handler->getMouseController()->detectMouseRegionFromClientCoordinates(mouseX, mouseY);
			handler->getMouseController()->transitionMouseState(regionName, mouseX, mouseY);
		}
		
		// Camera lateral movement (panning)
		if (handler->getMouseController()->isShiftLeftClickActive() && handler->getMouseController()->getCurrentMouseState() == handler->getMouseController()->getAboveOverlayState())
		{
			// Lock state transitions during camera panning
			handler->setCameraOperationLocked(true);
			
			if (handler->getOverlay())
			{
				int deltaX = handler->getMouseController()->getMouseDeltaX();
				int deltaY = handler->getMouseController()->getMouseDeltaY();
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
		
		if (handler->getMouseController()->getCurrentMouseState() == handler->getMouseController()->getAboveOverlayState())
		{
			if (handler->getMouseController()->hasClickEvent())
			{
				if (handler->getOverlay())
				{
					int mouseX = handler->getMouseController()->getCurrentMouseX();
					int mouseY = handler->getMouseController()->getCurrentMouseY();
					handler->getOverlay()->shootRaycastFromUIHandler(mouseX, mouseY);
				}
			}
		}
		
		const bool* keystate = SDL_GetKeyboardState(NULL);
		bool isShiftPressed = (keystate[SDL_SCANCODE_LSHIFT] || keystate[SDL_SCANCODE_RSHIFT]);

		if (handler->getMouseController()->isLeftButtonClicking() && !isShiftPressed && handler->getMouseController()->getCurrentMouseState() == handler->getMouseController()->getAboveOverlayState())
		{
			if (handler->getMouseController()->isClickHeldForDuration(200))
			{
				if (handler->getOverlay() && !handler->getOverlay()->isGizmoActive())
				{
					int deltaX = handler->getMouseController()->getMouseDeltaX();
					int deltaY = handler->getMouseController()->getMouseDeltaY();
					handler->getOverlay()->ProcessCameraOrbiting(deltaX, deltaY);
				}
			}
		}
		
		// Process mouse wheel input ONLY when mouse is above overlay
		if (handler->getMouseController()->getCurrentMouseState() == handler->getMouseController()->getAboveOverlayState())
		{
			if (handler->getMouseController()->hasWheelInput())
			{
				int wheelDirection = handler->getMouseController()->getMouseWheelDirection();
				handler->getOverlay()->ProcessZoom(wheelDirection);
			}
		}
		else
		{
			// If mouse is not above overlay, clear any pending wheel input
			if (handler->getMouseController()->hasWheelInput())
			{
				handler->getMouseController()->resetWheelDirection();
			}
		}
		
		if (handler->getOverlay() && handler->getMouseController())
		{
			SDL_Window* overlayWindow = handler->getOverlay()->getHandle();

			if (overlayWindow)
			{
				// Get mouse position relative to the overlay window
				float mouseX, mouseY;
				Uint32 mouseState = SDL_GetMouseState(&mouseX, &mouseY);
				
				bool leftDown = handler->getMouseController()->isLeftButtonClicking();
				bool rightDown = (mouseState & SDL_BUTTON_RMASK) != 0;
				bool middleDown = (mouseState & SDL_BUTTON_MMASK) != 0;
					
				float wheelDelta = 0.0f;

				// if (handler->getMouseController()->hasWheelInput())
				// {
				// 	wheelDelta = static_cast<float>(handler->getMouseController()->getMouseWheelDirection());
				// }
					
				handler->getOverlay()->injectMouseInputs(
					static_cast<int>(mouseX),
					static_cast<int>(mouseY),
					leftDown,
					rightDown,
					middleDown,
					wheelDelta
				);
				
				// handler->getOverlay()->render();
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
	
	return interval; 

	
}


// -------------------- End of Render Timer Callback --------------------

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


// ---------- Scene Update ------------ 

void UIHandler::initializeSceneObjects() 
{
	if (scene_initialized_ || !vk_scene_ || !main_camera_) 
	{
		std::cout << "[UIHandler] Scene already initialized or missing objects" << std::endl;
		return;
	}
	
	if (!overlay_viewport_) 
	{
		std::cerr << "[UIHandler] ERROR: Overlay viewport not created yet!" << std::endl;
		return;
	}
	
	std::cout << "[UIHandler] Initializing scene objects with Vulkan context..." << std::endl;
	
	overlay_viewport_->makeContextCurrent();
	
	if (vk_scene_) 
	{
		vk_scene_->initialize();
	}
	
	if (vk_scene_) 
	{
		for (auto* obj : vk_scene_->getObjectsRef()) 
		{
			if (obj && obj->getIsMesh()) 
			{
				Mesh* mesh = static_cast<Mesh*>(obj);
				mesh->finalize();
			}
		}
	}
	
	if (vk_scene_ && main_camera_) 
	{
		overlay_viewport_->setVKScene(vk_scene_);
		std::cout << "[UIHandler] VKScene set to overlay viewport - Objects count: " << vk_scene_->getObjectsRef().size() << std::endl;
		std::cout << "[UIHandler] VKScene camera: " << (vk_scene_->getActiveCamera() ? vk_scene_->getActiveCamera()->getName() : "NULL") << std::endl;
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
	
	frame_datas_ = new SIMILI::Frontend::FrameDatas(this);
	std::cout << "[UIHandler] FrameDatas initialized with UIHandler" << std::endl;
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

void UIHandler::set_MouseControl(SIMILI::Input::MouseController* mouseControl)
{
	if (mouse_controller_ == mouseControl)
	{
		return;
	}

	if (mouse_controller_)
	{
		delete mouse_controller_;
		mouse_controller_ = nullptr;
	}

	mouse_controller_ = mouseControl;

	if (mouse_controller_)
	{
		if (parent_window_)
		{
			mouse_controller_->setWindowHandle(parent_window_);
		}
	}
}


void UIHandler::captureIFramePositions()
{
	if (!frame_datas_)
	{
		std::cout << "[UIHandler] Cannot capture iframe positions - frame_datas_ not available" << std::endl;
		return;
	}

	if (!parent_sdl_window_)
	{
		std::cout << "[UIHandler] parent_sdl_window_ not available" << std::endl;
		return;
	}

	SDL_Window* sdlWindow = parent_sdl_window_->getHandle();
	if (!sdlWindow)
	{
		std::cout << "[UIHandler] SDL window handle not available" << std::endl;
		return;
	}

	if (mouse_controller_)
	{
		mouse_controller_->setWindowHandle(sdlWindow);
		
		bool isMaximized = parent_sdl_window_->isMaximized();
		int left = 0, top = 0, right = 0, bottom = 0;
		if (isMaximized)
		{
			parent_sdl_window_->getBorderOffsets(left, top, right, bottom);
		}
		mouse_controller_->setMaximizedState(isMaximized, left, top);
	}

	frame_datas_->catchFrameData(sdlWindow);

	if (mouse_controller_)
	{
		const auto& frameDataMap = frame_datas_->getFrameData();
		
		if (frameDataMap.empty())
		{
			std::cout << "[UIHandler] WARNING: No frame data captured from FrameDatas" << std::endl;
			return;
		}

		std::map<std::string, SIMILI::Input::MouseControlFrameData> mouseControlDataMap;
		for (const auto& pair : frameDataMap)
		{
			const auto& data = pair.second;
			SIMILI::Input::MouseControlFrameData convertedData;
			convertedData.name = data.name;
			convertedData.clientX = data.clientX;
			convertedData.clientY = data.clientY;
			convertedData.width = data.width;
			convertedData.height = data.height;
			mouseControlDataMap[pair.first] = convertedData;
			
			std::cout << "[UIHandler] Panel '" << data.name << "' bounds: "
				<< "clientX=" << data.clientX << " clientY=" << data.clientY
				<< " width=" << data.width << " height=" << data.height << std::endl;
		}

		mouse_controller_->updateMouseControlPanelBoundsFromFrameData(mouseControlDataMap);
	}
		
	if (overlay_viewport_ && vk_scene_ && vk_scene_->getActiveCamera())
	{
		SIMILI::Frontend::IFrameScreenData ViewportPanelSize;

		if (frame_datas_->getFrameData("viewport_panel", ViewportPanelSize))
		{
			float dpiScale = ViewportPanelSize.dpiScale;
			int Width = static_cast<int>(ViewportPanelSize.width * dpiScale);
			int Height = static_cast<int>(ViewportPanelSize.height * dpiScale);
			
			Camera* cam = vk_scene_->getActiveCamera();

			cam->setResolution(Width, Height, dpiScale);

			std::cout << "[UIHandler] Camera resolution updated: " << "x : " << ViewportPanelSize.height << " y : " << ViewportPanelSize.width
			<< " DPI scale: " << dpiScale  << std::endl;
			
			overlay_viewport_->updateViewportDimensions(Width, Height);

			if (composite_test_renderer_ && composite_test_renderer_->needs_repositioning())
			{
				composite_test_renderer_->UpdateParentData(
					ViewportPanelSize.clientX,
					ViewportPanelSize.clientY,
					ViewportPanelSize.width,
					ViewportPanelSize.height,
					dpiScale
				);

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
			}

		}
	}
}

void UIHandler::CallTestFromServer()
{
	
	if (!CefCurrentlyOn(TID_UI))
	{
		CefPostTask(TID_UI, base::BindOnce(&UIHandler::enableSlotTextureRendering, base::Unretained(this), true));
		return;
	}
	
	enableSlotTextureRendering(true);

}