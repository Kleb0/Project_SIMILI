#include "ui_handler.hpp"
#include "CEF_Resizer.hpp"
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
#include "ResourcesLoader.hpp"
#include "include/base/cef_callback.h"
#include "include/wrapper/cef_closure_task.h"
#include <iostream>
#include <algorithm>
#include <set>
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
	last_viewport_width_(0), last_viewport_height_(0), current_window_width_(1920), current_window_height_(1080), 
	last_frame_capture_time_(), vk_scene_(nullptr),
	vk_renderer_(nullptr), vulkan_pipelines_(nullptr), main_camera_(nullptr), cube_mesh_ptr_(nullptr), scene_initialized_(false),
	render_message_router_(nullptr), 
	frame_datas_(nullptr),
	ui_manager_(nullptr),
	window_delegate_(nullptr),
	mouse_controller_(nullptr),
	is_camera_operation_locked_(false),
	slot_texture_renderer_(nullptr),
	composite_test_renderer_(nullptr),
	cef_drawer_(nullptr),
	splitter_(nullptr),
	owner_thread_id_(std::this_thread::get_id()),
	pending_iframe_capture_(false),
	pending_ui_panel_cache_(false),
	resource_request_handler_(nullptr)
{
	mouse_controller_ = new SIMILI::Input::MouseController();
	ui_manager_ = nullptr;
	
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
	// ui_manager_ is not owned by UIHandler, so we don't delete it
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
		cef_drawer_ = nullptr;
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
	
	// Allow localhost requests and disable security for development
	command_line->AppendSwitch("allow-running-insecure-content");
	command_line->AppendSwitch("disable-web-security");
	command_line->AppendSwitch("allow-file-access-from-files");
	command_line->AppendSwitch("allow-file-access");
	command_line->AppendSwitch("disable-site-isolation-trials");
	command_line->AppendSwitch("disable-same-origin-policy");
	command_line->AppendSwitch("allow-insecure-localhost");
	command_line->AppendSwitch("ignore-certificate-errors-spki-list");
	command_line->AppendSwitch("ignore-ssl-errors");
	
	std::cout << "[UIHandler] CEF command line flags configured for localhost access" << std::endl;
}

void UIHandler::OnContextInitialized() 
{
	if (!resource_request_handler_)
	{
		resource_request_handler_ = ResourcesLoader::createRequestHandler();
		std::cout << "[UIHandler] Resource handler preloaded at initialization" << std::endl;
	}
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

CefRefPtr<CefRequestHandler> UIHandler::GetRequestHandler()
{
	return this;
}

CefRefPtr<CefResourceRequestHandler> UIHandler::GetResourceRequestHandler(
	CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame,
	CefRefPtr<CefRequest> request,
	bool is_navigation,
	bool is_download,
	const CefString& request_initiator,
	bool& disable_default_handling)
{
	if (!resource_request_handler_)
	{
		resource_request_handler_ = ResourcesLoader::createRequestHandler();
		std::cout << "[UIHandler] Resource handler created and cached" << std::endl;
	}
	
	return resource_request_handler_;
}

void UIHandler::initializeDefaultUIPanels()
{
	if (ui_manager_)
	{
		ui_manager_->initializeDefaultUIPanels();
	}
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
	if (handler)
	{
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
			handler->setCameraOperationLocked(true);
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
				// if (handler->getOverlay())
				// {
				// 	int mouseX = handler->getMouseController()->getCurrentMouseX();
				// 	int mouseY = handler->getMouseController()->getCurrentMouseY();
				// 	handler->getOverlay()->shootRaycastFromUIHandler(mouseX, mouseY);
				// }
			}
		}
		
		const bool* keystate = SDL_GetKeyboardState(NULL);
		bool isShiftPressed = (keystate[SDL_SCANCODE_LSHIFT] || keystate[SDL_SCANCODE_RSHIFT]);

		if (handler->getMouseController()->isLeftButtonClicking() && !isShiftPressed && handler->getMouseController()->getCurrentMouseState() == handler->getMouseController()->getAboveOverlayState())
		{
			if (handler->getMouseController()->isClickHeldForDuration(200))
			{
				// if (handler->getOverlay() && !handler->getOverlay()->isGizmoActive())
				// {
				// 	int deltaX = handler->getMouseController()->getMouseDeltaX();
				// 	int deltaY = handler->getMouseController()->getMouseDeltaY();
				// 	handler->getOverlay()->ProcessCameraOrbiting(deltaX, deltaY);
				// }
			}
		}
		
		// Process mouse wheel input ONLY when mouse is above overlay
		if (handler->getMouseController()->getCurrentMouseState() == handler->getMouseController()->getAboveOverlayState())
		{
			if (handler->getMouseController()->hasWheelInput())
			{
				int wheelDirection = handler->getMouseController()->getMouseWheelDirection();
				// handler->getOverlay()->ProcessZoom(wheelDirection);
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
		
		// if (handler->getOverlay() && handler->getMouseController())
		// {
		// 	SDL_Window* overlayWindow = handler->getOverlay()->getHandle();

		// 	if (overlayWindow)
		// 	{
		// 		// Get mouse position relative to the overlay window
		// 		float mouseX, mouseY;
		// 		Uint32 mouseState = SDL_GetMouseState(&mouseX, &mouseY);
				
		// 		bool leftDown = handler->getMouseController()->isLeftButtonClicking();
		// 		bool rightDown = (mouseState & SDL_BUTTON_RMASK) != 0;
		// 		bool middleDown = (mouseState & SDL_BUTTON_MMASK) != 0;
					
		// 		float wheelDelta = 0.0f;

		// 		// if (handler->getMouseController()->hasWheelInput())
		// 		// {
		// 		// 	wheelDelta = static_cast<float>(handler->getMouseController()->getMouseWheelDirection());
		// 		// }
					
		// 		handler->getOverlay()->injectMouseInputs(
		// 			static_cast<int>(mouseX),
		// 			static_cast<int>(mouseY),
		// 			leftDown,
		// 			rightDown,
		// 			middleDown,
		// 			wheelDelta
		// 		);
				
		// 		// handler->getOverlay()->render();
		// 	}
		// }
		
		if (parentWindow)
		{
			auto frameDataMap = handler->getAllIFrames();
			parentWindow->renderThreeDScreen(frameDataMap);
		}
	}
	
	return interval; 	
}


// -------------------- End of Render Timer Callback --------------------

// bool UIHandler::isOverlayRenderingEnabled() const 
// {
// 	if (overlay_viewport_) 
// 	{
// 		return overlay_viewport_->isRenderingEnabled();
// 	}
// 	return false;
// }

void UIHandler::enableSlotTextureRendering(bool enable)
{
	if (!parent_window_) return;
	
	if (enable) 
	{
		if (!slot_texture_renderer_) 
		{
			SDL_GLContext shareContext = nullptr;
			// if (overlay_viewport_)
			// {
			// 	overlay_viewport_->makeContextCurrent();
			// 	shareContext = overlay_viewport_->getGLContext();
			// }
			
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
	
	// if (!overlay_viewport_) 
	// {
	// 	std::cerr << "[UIHandler] ERROR: Overlay viewport not created yet!" << std::endl;
	// 	return;
	// }
	
	std::cout << "[UIHandler] Initializing scene objects with Vulkan context..." << std::endl;
	
	// overlay_viewport_->makeContextCurrent();
	
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
		// overlay_viewport_->setVKScene(vk_scene_);
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
	
	if (obj->getIsMesh())
	{
		Mesh* mesh = static_cast<Mesh*>(obj);
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

bool UIHandler::isOwnerThread() const
{
	return std::this_thread::get_id() == owner_thread_id_;
}

bool UIHandler::hasRuntimeLayoutChanged(const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& beforeMap, const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& afterMap) const
{
	if (beforeMap.size() != afterMap.size())
	{
		return true;
	}

	for (const auto& pair : afterMap)
	{
		auto beforeIt = beforeMap.find(pair.first);
		if (beforeIt == beforeMap.end())
		{
			return true;
		}

		const SIMILI::Frontend::IFrameScreenData& beforeFrame = beforeIt->second;
		const SIMILI::Frontend::IFrameScreenData& afterFrame = pair.second;
		
		if (beforeFrame.relativeX != afterFrame.relativeX || beforeFrame.relativeY != afterFrame.relativeY || 
		    beforeFrame.width != afterFrame.width || beforeFrame.height != afterFrame.height)
		{
			return true;
		}
	}

	return false;
}

std::map<std::string, CEF_Drawer::UIPanelFrameData> UIHandler::buildRuntimeLayoutFrameMap(const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& frameDataMap) const
{
	std::map<std::string, CEF_Drawer::UIPanelFrameData> layoutFrameMap;

	for (const auto& pair : frameDataMap)
	{
		CEF_Drawer::UIPanelFrameData panelFrame;
		panelFrame.x = pair.second.relativeX;
		panelFrame.y = pair.second.relativeY;
		panelFrame.width = pair.second.width;
		panelFrame.height = pair.second.height;
		layoutFrameMap[pair.first] = panelFrame;
	}

	return layoutFrameMap;
}

void UIHandler::processPendingFrameUpdates()
{
	if (!isOwnerThread())
	{
		return;
	}

	if (pending_iframe_capture_.exchange(false))
	{
		captureIFramePositions();
		pending_ui_panel_cache_.store(false);
		return;
	}

	if (pending_ui_panel_cache_.exchange(false))
	{
		cacheUIPanelFrameDatas();
	}
}

void UIHandler::startManager(VKContext* vkContext, VkRenderPass renderPass)
{
	if (!parent_window_ || !vkContext || renderPass == VK_NULL_HANDLE)
	{
		std::cerr << "[UIHandler] Cannot start Manager: missing resources" << std::endl;
		return;
	}

	if (ui_manager_)
	{
		ui_manager_->setVKContext(vkContext);
		ui_manager_->setVulkanPipelines(vulkan_pipelines_);
		ui_manager_->setRenderPass(renderPass);
		ui_manager_->setCEFDrawer(cef_drawer_);
		std::cout << "[UIHandler] UIManager initialized with Vulkan resources" << std::endl;
	}

	if (!splitter_)
	{
		splitter_ = std::make_unique<Splitter>();
	}



	if (vulkan_pipelines_)
	{
		splitter_->setVulkanPipelines(vulkan_pipelines_);
		std::cout << "[UIHandler] VulkanPipelines set for splitter" << std::endl;
	}
	else
	{
		std::cerr << "[UIHandler] ERROR: No VulkanPipelines available for splitter" << std::endl;
		splitter_.reset();
		return;
	}



	std::cout << "[UIHandler] Splitter initialized" << std::endl;

	if (frame_datas_)
	{
		// splitter_->syncFrameDatas(frame_datas_);
		cacheUIPanelFrameDatas();
	}
}

bool UIHandler::handleSplitterEvent(const SDL_Event& event)
{
	if (!splitter_)
	{
		return false;
	}

	const bool handled = false; // splitter_->handleEvent(event);
	
	const bool isWindowResizeEvent = (event.type == SDL_EVENT_WINDOW_RESIZED ||
	                                   event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED ||
	                                   event.type == SDL_EVENT_WINDOW_MAXIMIZED ||
	                                   event.type == SDL_EVENT_WINDOW_RESTORED);
	
	if (handled)
	{
		cacheUIPanelFrameDatas();
	}
	else if (isWindowResizeEvent && cef_drawer_)
	{
		// const auto currentFrameDataMap = splitter_->getAllFrameDatas();
		// if (!currentFrameDataMap.empty())
		// {
		// 	cef_drawer_->requestRuntimeLayoutSync(buildRuntimeLayoutFrameMap(currentFrameDataMap));
		// 	cef_drawer_->getResizer().forceLayoutSync();
		// 	cacheUIPanelFrameDatas();
		// }
	}

	return handled;
}

bool UIHandler::getResolvedViewportFrameData(SIMILI::Frontend::IFrameScreenData& outData) const
{

	if (!frame_datas_)
	{
		return false;
	}

	return frame_datas_->getFrameData("viewport_panel", outData);
}

std::map<std::string, SIMILI::Frontend::IFrameScreenData> UIHandler::getRuntimeFrameDataMap() const
{
	if (splitter_)
	{
		std::map<std::string, SIMILI::Frontend::IFrameScreenData> frameDataMap; // splitter_->getAllFrameDatas();
		if (!frameDataMap.empty())
		{
			return frameDataMap;
		}
	}

	if (!frame_datas_)
	{
		return {};
	}

	return frame_datas_->getFrameData();
}


void UIHandler::captureIFramePositions()
{
	if (!isOwnerThread())
	{
		pending_iframe_capture_.store(true);
		return;
	}

	const std::chrono::milliseconds FRAME_CAPTURE_DEBOUNCE_MS(500);
	auto currentTime = std::chrono::steady_clock::now();
	auto timeSinceLastCapture = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - last_frame_capture_time_);
	
	if (last_frame_capture_time_.time_since_epoch().count() > 0 && timeSinceLastCapture < FRAME_CAPTURE_DEBOUNCE_MS)
	{
		return;
	}

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
	
	last_frame_capture_time_ = currentTime;
	
	if (!splitter_ && !iframe_data_map_.empty() && parent_sdl_window_)
	{
		VkRenderPass renderPass = parent_sdl_window_->getRenderPass();
		if (vk_renderer_ && renderPass != VK_NULL_HANDLE)
		{
			std::cout << "\n  [UIHANDLER] --------------------------------- SPLITTER CREATION TRIGGERED ---------------------------------" << std::endl;
			std::cout << "[UIHandler] Frame data available - initializing splitter now" << std::endl;
			startManager(vk_renderer_->getContext(), renderPass);
		}
	}
	
	if (ui_manager_)
	{
		ui_manager_->syncFrameDatas(frame_datas_, sdlWindow);
		std::cout << "[UIHandler] UI panel frame data synced to UIManager" << std::endl;
	}
	else
	{
		std::cout << "[UIHandler] WARNING: ui_manager_ is null, cannot sync frame data" << std::endl;
	}

	cacheUIPanelFrameDatas();
	
	if (parent_sdl_window_)
	{
		parent_sdl_window_->updateFrameDatas(frame_datas_);
	}

	if (mouse_controller_)
	{
		auto frameDataMap = getRuntimeFrameDataMap();
		
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
			
			std::cout << "\n [UIHandler] Panel '" << data.name << "' bounds: "
				<< "clientX=" << data.clientX << " clientY=" << data.clientY
				<< " width=" << data.width << " height=" << data.height << std::endl;
		}

		mouse_controller_->updateMouseControlPanelBoundsFromFrameData(mouseControlDataMap);
	}
		
	// if (overlay_viewport_ && vk_scene_ && vk_scene_->getActiveCamera())
	// {
	// 	SIMILI::Frontend::IFrameScreenData ViewportPanelSize;

	// 	if (getResolvedViewportFrameData(ViewportPanelSize))
	// 	{
	// 		float dpiScale = ViewportPanelSize.dpiScale;
	// 		int Width = static_cast<int>(ViewportPanelSize.width * dpiScale);
	// 		int Height = static_cast<int>(ViewportPanelSize.height * dpiScale);
			
	// 		Camera* cam = vk_scene_->getActiveCamera();

	// 		cam->setResolution(Width, Height, dpiScale);

	// 		std::cout << "[UIHandler] Camera resolution updated: " << "x : " << ViewportPanelSize.height << " y : " << ViewportPanelSize.width
	// 		<< " DPI scale: " << dpiScale  << std::endl;
			
	// 		overlay_viewport_->updateViewportDimensions(Width, Height);

	// 		if (composite_test_renderer_ && composite_test_renderer_->needs_repositioning())
	// 		{
	// 			composite_test_renderer_->UpdateParentData(
	// 				ViewportPanelSize.clientX,
	// 				ViewportPanelSize.clientY,
	// 				ViewportPanelSize.width,
	// 				ViewportPanelSize.height,
	// 				dpiScale
	// 			);

	// 			switch (composite_test_renderer_->anchor_position())
	// 			{
	// 				case PanelAnchorPosition::Centerize:
	// 					composite_test_renderer_->setCenterize();
	// 					break;
	// 				case PanelAnchorPosition::TopLeft:
	// 					composite_test_renderer_->setTopLeft();
	// 					break;
	// 				case PanelAnchorPosition::TopRight:
	// 					composite_test_renderer_->setTopRight();
	// 					break;
	// 				case PanelAnchorPosition::BottomLeft:
	// 					composite_test_renderer_->setBottomLeft();
	// 					break;
	// 				case PanelAnchorPosition::BottomRight:
	// 					composite_test_renderer_->setBottomRight();
	// 					break;
	// 				case PanelAnchorPosition::MiddleLeft:
	// 					composite_test_renderer_->setMiddleLeft();
	// 					break;
	// 				case PanelAnchorPosition::MiddleRight:
	// 					composite_test_renderer_->setMiddleRight();
	// 					break;
	// 			}

	// 			composite_test_renderer_->clearRepositioningFlag();
	// 		}

	// 	}
	// }
}

void UIHandler::forceCaptureIFramePositions()
{
	if (!isOwnerThread())
	{
		pending_iframe_capture_.store(true);
		return;
	}

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
	
	last_frame_capture_time_ = std::chrono::steady_clock::now();
	
	if (!splitter_ && !iframe_data_map_.empty() && parent_sdl_window_)
	{
		VkRenderPass renderPass = parent_sdl_window_->getRenderPass();
		if (vk_renderer_ && renderPass != VK_NULL_HANDLE)
		{
			std::cout << "\n  [UIHANDLER] --------------------------------- UI MANAGER CREATION TRIGGERED ---------------------------------" << std::endl;
			std::cout << "[UIHandler] Frame data available - initializing UIManager now" << std::endl;		
		}
	}
	
	if (ui_manager_)
	{
		ui_manager_->syncFrameDatas(frame_datas_, sdlWindow);
		std::cout << "[UIHandler] FORCE - UI panel frame data synced to UIManager" << std::endl;
	}
	else
	{
		std::cout << "[UIHandler] FORCE - WARNING: ui_manager_ is null, cannot sync frame data" << std::endl;
	}

	cacheUIPanelFrameDatas();
	
	if (parent_sdl_window_)
	{
		parent_sdl_window_->updateFrameDatas(frame_datas_);
	}

	if (mouse_controller_)
	{
		auto frameDataMap = getRuntimeFrameDataMap();
		
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
		}

		mouse_controller_->updateMouseControlPanelBoundsFromFrameData(mouseControlDataMap);
	}
}

void UIHandler::updateWindowSize(int width, int height)
{
	current_window_width_ = width;
	current_window_height_ = height;
	std::cout << "[UIHandler] Window size updated to: " << width << "x" << height << std::endl;
}




void UIHandler::cacheUIPanelFrameDatas()
{
	if (!isOwnerThread())
	{
		pending_ui_panel_cache_.store(true);
		return;
	}

	if (ui_manager_ && frame_datas_)
	{
		auto frameDataMap = ui_manager_->getUIPanelFrameDatas();
		std::cout << "[UIHandler] cacheUIPanelFrameDatas: Retrieved " << frameDataMap.size() 
				  << " panel frame data from UIManager" << std::endl;
	}
}

void UIHandler::clearUIPanels()
{

	splitter_.reset();
	iframe_data_map_.clear();
	pending_iframe_capture_.store(false);
	pending_ui_panel_cache_.store(false);
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