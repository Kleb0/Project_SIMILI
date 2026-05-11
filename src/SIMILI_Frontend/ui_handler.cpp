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
#include <iomanip>
#include <vector>
#include <commctrl.h>  
#include <glm/glm.hpp>
#include <SDL3/SDL.h>

#pragma comment(lib, "comctl32.lib")

static std::unordered_map<SDL_TimerID, UIHandler*> g_timerHandlerMap;

static UIHandler* g_activeHandler = nullptr;

namespace
{
	const char* windowRenderStateToString(WindowRenderState state)
	{
		switch (state)
		{
			case WindowRenderState::Init:
				return "Init";
			case WindowRenderState::Maximized:
				return "Maximized";
			case WindowRenderState::Reduced:
				return "Reduced";
			default:
				return "Unknown";
		}
	}

	void printFrameDataSnapshot(const char* snapshotLabel, const SIMILI::Frontend::IFrameScreenData& data)
	{
		auto oldFlags = std::cout.flags();
		auto oldPrecision = std::cout.precision();

		std::cout << snapshotLabel << std::endl;
		std::cout << "Relative Position (CEF): X=" << data.relativeX << " Y=" << data.relativeY << std::endl;
		std::cout << "Client Position (CEF): X=" << data.clientX << " Y=" << data.clientY << std::endl;
		std::cout << "Size: W=" << data.width << " H=" << data.height << std::endl;
		std::cout << "Screen Position: X=" << data.screenX << " Y=" << data.screenY << std::endl;
		std::cout << "Window Position: X=" << data.windowX << " Y=" << data.windowY << std::endl;
		std::cout << "Window Size: W=" << data.windowWidth << " H=" << data.windowHeight << std::endl;
		std::cout << std::fixed << std::setprecision(2);
		std::cout << "DPI Scale: " << data.dpiScale << std::endl;
		std::cout.flags(oldFlags);
		std::cout.precision(oldPrecision);
		std::cout << "Screen Resolution: " << data.screenWidth << "x" << data.screenHeight << std::endl;
	}

	void printFrameDataDelta(const SIMILI::Frontend::IFrameScreenData& before, const SIMILI::Frontend::IFrameScreenData& after)
	{
		std::cout << "Delta: dRel(X=" << (after.relativeX - before.relativeX)
			<< " Y=" << (after.relativeY - before.relativeY)
			<< ") dClient(X=" << (after.clientX - before.clientX)
			<< " Y=" << (after.clientY - before.clientY)
			<< ") dSize(W=" << (after.width - before.width)
			<< " H=" << (after.height - before.height)
			<< ") dWindow(W=" << (after.windowWidth - before.windowWidth)
			<< " H=" << (after.windowHeight - before.windowHeight)
			<< ")" << std::endl;
	}

	void logFrameMapComparison(
		const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& beforeMap,
		const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& afterMap,
		const std::string& title)
	{
		std::set<std::string> frameNames;
		for (const auto& pair : beforeMap)
		{
			if (pair.first != "viewport_panel")
			{
				frameNames.insert(pair.first);
			}
		}
		for (const auto& pair : afterMap)
		{
			if (pair.first != "viewport_panel")
			{
				frameNames.insert(pair.first);
			}
		}

		std::cout << std::endl;
		std::cout << "========== " << title << " ==========" << std::endl;

		for (const auto& frameName : frameNames)
		{
			auto beforeIt = beforeMap.find(frameName);
			auto afterIt = afterMap.find(frameName);

			std::cout << std::endl;
			std::cout << "--- Frame: " << frameName << " ---" << std::endl;

			if (beforeIt == beforeMap.end())
			{
				std::cout << "Before: missing" << std::endl;
			}
			else
			{
				printFrameDataSnapshot("Before:", beforeIt->second);
			}

			if (afterIt == afterMap.end())
			{
				std::cout << "After: missing" << std::endl;
			}
			else
			{
				printFrameDataSnapshot("After:", afterIt->second);
			}

			if (beforeIt != beforeMap.end() && afterIt != afterMap.end())
			{
				printFrameDataDelta(beforeIt->second, afterIt->second);
			}
		}

		std::cout << std::endl;
		std::cout << "========================================================" << std::endl;
	}

	void logSplitterRedrawDiagnostics(
		SDL_ApplicationWindow* sdlWindow,
		const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& beforeCapturedFrames,
		const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& afterCapturedFrames,
		const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& beforeGeometryFrames,
		const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& afterGeometryFrames)
	{
		int logicalW = 0;
		int logicalH = 0;
		WindowRenderState state = WindowRenderState::Init;

		if (sdlWindow && sdlWindow->getHandle())
		{
			SDL_GetWindowSize(sdlWindow->getHandle(), &logicalW, &logicalH);
			state = sdlWindow->getWindowRenderState();
		}

		std::cout << std::endl;
		std::cout << "========== Compare build with SDL3 window state at "
			<< windowRenderStateToString(state)
			<< " | size " << logicalW << "x" << logicalH
			<< " ==========" << std::endl;

		// logFrameMapComparison(beforeCapturedFrames, afterCapturedFrames, "FrameDatas - Captured Frame Data Comparison");
		// logFrameMapComparison(beforeGeometryFrames, afterGeometryFrames, "UIManager - Geometry Frame Data Comparison");
	}
}

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
	splitter_(nullptr),
	owner_thread_id_(std::this_thread::get_id()),
	pending_iframe_capture_(false),
	pending_ui_panel_cache_(false),
	pending_deferred_layout_refresh_(false),
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
				return RenderTimerProc(param, timerID, interval, handler->parent_sdl_window_);
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
	if (parent_sdl_window_)
		return parent_sdl_window_->getRenderHandler();
	return nullptr;
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


static Uint32 SDLCALL RenderTimerProc(void* param, SDL_TimerID timerID, Uint32 interval, SDL_ApplicationWindow* parentWindow) 
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
		
		// if (parentWindow)
		// {
		// 	auto frameDataMap = handler->getAllIFrames();
		// 	parentWindow->renderThreeDScreen(frameDataMap);
		// }
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

	if (pending_deferred_layout_refresh_.exchange(false))
	{
		std::map<std::string, SIMILI::Frontend::IFrameScreenData> beforeCapturedFrames;
		std::map<std::string, SIMILI::Frontend::IFrameScreenData> beforeGeometryFrames;
		if (frame_datas_)
		{
			beforeCapturedFrames = frame_datas_->getFrameData();
		}
		if (ui_manager_)
		{
			beforeGeometryFrames = ui_manager_->getUIPanelFrameDatas();
		}

		// ------ Redraw Texture at Splitter action ------ //
		// The injected DOM layout has settled; recapture the browser bounds, rebuild
		// FrameDatas, then refresh panel UV/layout before the follow-up repaint.
		forceCaptureIFramePositions();
		if (ui_manager_)
		{
			ui_manager_->refreshPanelTextureLayout(false);
		}

		std::map<std::string, SIMILI::Frontend::IFrameScreenData> afterCapturedFrames;
		std::map<std::string, SIMILI::Frontend::IFrameScreenData> afterGeometryFrames;
		if (frame_datas_)
		{
			afterCapturedFrames = frame_datas_->getFrameData();
		}
		if (ui_manager_)
		{
			afterGeometryFrames = ui_manager_->getUIPanelFrameDatas();
		}

		logSplitterRedrawDiagnostics(
			parent_sdl_window_,
			beforeCapturedFrames,
			afterCapturedFrames,
			beforeGeometryFrames,
			afterGeometryFrames);

		if (parent_sdl_window_)
		{
			parent_sdl_window_->requestBrowserRepaint();
		}
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
	else if (isWindowResizeEvent)
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

void UIHandler::finalizeDeferredLayoutRefresh(CefRefPtr<CefBrowser> delayedBrowser)
{
	// ------ Redraw Texture at Splitter action ------ //
	pending_deferred_layout_refresh_.store(true);

	if (delayedBrowser && delayedBrowser->GetHost())
	{
		delayedBrowser->GetHost()->Invalidate(PET_VIEW);
	}
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

	if (ui_manager_)
	{
		// ------ Redraw Texture at Splitter action ------ //
		// Pull the splitter-resolved iframe map produced by PanelMapBuilder so the
		// next FrameDatas capture uses the same geometry as the incoming CEF redraw.
		auto iframeDataMap = ui_manager_->getResolvedUIPanelIFrames();
		if (iframeDataMap.empty())
		{
			std::cout << "[UIHandler] cacheUIPanelFrameDatas: No panel frame data available from UIManager" << std::endl;
			return;
		}

		const auto currentIFrames = getAllIFrames();
		bool hasPanelLayoutChange = false;

		for (const auto& pair : iframeDataMap)
		{
			auto currentIt = currentIFrames.find(pair.first);
			if (currentIt == currentIFrames.end() ||
				currentIt->second.x != pair.second.x ||
				currentIt->second.y != pair.second.y ||
				currentIt->second.width != pair.second.width ||
				currentIt->second.height != pair.second.height ||
				currentIt->second.clientX != pair.second.clientX ||
				currentIt->second.clientY != pair.second.clientY)
			{
				hasPanelLayoutChange = true;
				iframe_data_map_[pair.first] = pair.second;
			}
		}

		ui_manager_->updateUIPanelIFrames(iframeDataMap);

		if (hasPanelLayoutChange && frame_datas_ && parent_window_)
		{
			// ------ Redraw Texture at Splitter action ------ //
			// Rebuild FrameDatas from the updated iframe map, then push the captured
			// bounds back into UIManager so UVs and geometry stay in sync.
			frame_datas_->catchFrameData(parent_window_);
			ui_manager_->cacheUIPanelFrameDatas(frame_datas_, {});
			std::cout << "[UIHandler] cacheUIPanelFrameDatas: Refreshed FrameDatas with " << iframeDataMap.size()
				      << " splitter-adjusted UI panels" << std::endl;
		}
		else
		{
			std::cout << "[UIHandler] cacheUIPanelFrameDatas: UI panel layout already up to date (" << iframeDataMap.size()
				      << " panels)" << std::endl;
		}
	}
}


void UIHandler::syncBrowserPanelLayoutFromCurrentFrames(WindowRenderState windowState, int currentWidth, int currentHeight, int referenceWidth, int referenceHeight)
{
	if (!ui_manager_)
	{
		std::cout << "[UIHandler] syncBrowserPanelLayoutFromCurrentFrames: skipped, ui_manager_ is null" << std::endl;
		return;
	}

	const auto iframeDataMap = ui_manager_->getResolvedUIPanelIFrames();
	if (iframeDataMap.empty())
	{
		std::cout << "[UIHandler] syncBrowserPanelLayoutFromCurrentFrames: skipped, no resolved UI panel frames" << std::endl;
		return;
	}

	if (!CefCurrentlyOn(TID_UI))
	{
		CefPostTask(TID_UI, base::BindOnce(&UIHandler::syncBrowserPanelLayoutFromCurrentFrames, base::Unretained(this), windowState, currentWidth, currentHeight, referenceWidth, referenceHeight));
		return;
	}

	if (!parent_sdl_window_)
	{
		std::cout << "[UIHandler] syncBrowserPanelLayoutFromCurrentFrames: skipped, SDL parent window is null" << std::endl;
		return;
	}

	CefRefPtr<CefBrowser> browser = parent_sdl_window_->getBrowser();
	if (!browser)
	{
		std::cout << "[UIHandler] syncBrowserPanelLayoutFromCurrentFrames: skipped, SDL browser is null" << std::endl;
		return;
	}

	CefRefPtr<CefFrame> mainFrame = browser->GetMainFrame();
	if (!mainFrame || !mainFrame->IsValid())
	{
		std::cout << "[UIHandler] syncBrowserPanelLayoutFromCurrentFrames: skipped, main frame is invalid" << std::endl;
		return;
	}

	const auto leftColumnIt = std::min_element(
		iframeDataMap.begin(), iframeDataMap.end(),
		[](const auto& lhs, const auto& rhs)
		{
			if (lhs.second.x != rhs.second.x)
			{
				return lhs.second.x < rhs.second.x;
			}
			return lhs.second.y < rhs.second.y;
		});

	const auto rightColumnIt = std::min_element(
		iframeDataMap.begin(), iframeDataMap.end(),
		[](const auto& lhs, const auto& rhs)
		{
			if (lhs.second.x != rhs.second.x)
			{
				return lhs.second.x > rhs.second.x;
			}
			return lhs.second.y < rhs.second.y;
		});

	const auto bottomPanelIt = std::max_element(
		iframeDataMap.begin(), iframeDataMap.end(),
		[](const auto& lhs, const auto& rhs)
		{
			if (lhs.second.width != rhs.second.width)
			{
				return lhs.second.width < rhs.second.width;
			}
			return lhs.second.y < rhs.second.y;
		});

	if (leftColumnIt == iframeDataMap.end() || rightColumnIt == iframeDataMap.end() || bottomPanelIt == iframeDataMap.end())
	{
		std::cout << "[UIHandler] syncBrowserPanelLayoutFromCurrentFrames: skipped, unable to derive layout anchors" << std::endl;
		return;
	}

	int topRowHeight = 0;
	for (const auto& pair : iframeDataMap)
	{
		if (pair.first == bottomPanelIt->first)
		{
			continue;
		}

		topRowHeight = (std::max)(topRowHeight, pair.second.height);
	}

	if (topRowHeight <= 0)
	{
		topRowHeight = leftColumnIt->second.height;
	}

	auto rightTopPanelIt = iframeDataMap.end();
	auto rightBottomPanelIt = iframeDataMap.end();
	for (auto it = iframeDataMap.begin(); it != iframeDataMap.end(); ++it)
	{
		if (it->first == bottomPanelIt->first)
		{
			continue;
		}

		if (it->second.x != rightColumnIt->second.x)
		{
			continue;
		}

		if (rightTopPanelIt == iframeDataMap.end() || it->second.y < rightTopPanelIt->second.y)
		{
			rightTopPanelIt = it;
		}

		if (rightBottomPanelIt == iframeDataMap.end() || it->second.y > rightBottomPanelIt->second.y)
		{
			rightBottomPanelIt = it;
		}
	}

	float scaleX = 1.0f;
	float scaleY = 1.0f;

	if (windowState == WindowRenderState::Maximized || windowState == WindowRenderState::Reduced)
	{
		if (referenceWidth > 0 && referenceHeight > 0 && currentWidth > 0 && currentHeight > 0)
		{
			scaleX = static_cast<float>(currentWidth) / static_cast<float>(referenceWidth);
			scaleY = static_cast<float>(currentHeight) / static_cast<float>(referenceHeight);
			std::cout << "[UIHandler] Scaling CEF dimensions: scaleX=" << scaleX << " scaleY=" << scaleY << std::endl;
		}
	}

	auto selectorForPanel = [](const std::string& panelName)
	{
		std::string selector = panelName;
		std::replace(selector.begin(), selector.end(), '_', '-');
		return selector;
	};

	const std::string leftPanelSelector = selectorForPanel(leftColumnIt->first);
	const std::string bottomPanelSelector = selectorForPanel(bottomPanelIt->first);
	const std::string rightTopPanelSelector =
		(rightTopPanelIt != iframeDataMap.end()) ? selectorForPanel(rightTopPanelIt->first) : std::string();
	const std::string rightBottomPanelSelector =
		(rightBottomPanelIt != iframeDataMap.end()) ? selectorForPanel(rightBottomPanelIt->first) : std::string();
	const int leftWidth = leftColumnIt->second.width;
	const int leftHeight = leftColumnIt->second.height;
	const int rightWidth = rightColumnIt->second.width;
	const int rightTopHeight = (rightTopPanelIt != iframeDataMap.end()) ? rightTopPanelIt->second.height : 0;
	const int rightBottomHeight = (rightBottomPanelIt != iframeDataMap.end()) ? rightBottomPanelIt->second.height : 0;
	const int bottomHeight = bottomPanelIt->second.height;

	auto buildFixedWidthBlock = [](const char* elementName, int width)
	{
		std::ostringstream block;
		block << "if(" << elementName << "){"
			<< elementName << ".style.width='" << width << "px';"
			<< elementName << ".style.minWidth='" << width << "px';"
			<< elementName << ".style.maxWidth='" << width << "px';"
			<< elementName << ".style.flex='0 0 " << width << "px';"
			<< elementName << ".style.overflow='hidden';}";
		return block.str();
	};

	auto buildFixedHeightBlock = [](const char* elementName, int height)
	{
		std::ostringstream block;
		block << "if(" << elementName << "){"
			<< elementName << ".style.height='" << height << "px';"
			<< elementName << ".style.minHeight='" << height << "px';"
			<< elementName << ".style.maxHeight='" << height << "px';"
			<< elementName << ".style.flex='0 0 " << height << "px';"
			<< elementName << ".style.overflow='hidden';}";
		return block.str();
	};

	auto buildFixedPanelBlock = [](const char* elementName, int width, int height)
	{
		std::ostringstream block;
		block << "if(" << elementName << "){"
			<< elementName << ".style.width='" << width << "px';"
			<< elementName << ".style.height='" << height << "px';"
			<< elementName << ".style.minWidth='" << width << "px';"
			<< elementName << ".style.minHeight='" << height << "px';"
			<< elementName << ".style.maxWidth='" << width << "px';"
			<< elementName << ".style.maxHeight='" << height << "px';"
			<< elementName << ".style.flex='0 0 auto';}";
		return block.str();
	};

	std::ostringstream script;
	script
		<< "(function(){"
		<< "const currentWidth=" << currentWidth << ";"
		<< "const currentHeight=" << currentHeight << ";"
		<< "const referenceWidth=" << referenceWidth << ";"
		<< "const referenceHeight=" << referenceHeight << ";"
		<< "const layoutScaleX=" << scaleX << ";"
		<< "const layoutScaleY=" << scaleY << ";"
		<< "const isInitState=" << (windowState == WindowRenderState::Init ? "true" : "false") << ";"
		<< "const findPanelElement=function(selector){"
		<< "return document.querySelector('.'+selector) || document.querySelector('.'+selector.replace(/-UI/g,'_UI'));"
		<< "};"
		<< "const resetBox=function(element){"
		<< "if(!element){return null;}"
		<< "element.style.width='';"
		<< "element.style.height='';"
		<< "element.style.flex='';"
		<< "element.style.minWidth='';"
		<< "element.style.minHeight='';"
		<< "element.style.maxWidth='';"
		<< "element.style.maxHeight='';"
		<< "element.style.alignSelf='';"
		<< "return element;"
		<< "};"
		<< "const clampPanel=function(selector){"
		<< "const element=resetBox(findPanelElement(selector));"
		<< "if(!element){return null;}"
		<< "element.style.overflow='hidden';"
		<< "element.style.margin='0';"
		<< "element.style.boxSizing='border-box';"
		<< "return element;"
		<< "};"
		<< "const root=document.documentElement;"
		<< "const body=document.body;"
		<< "const mainContainer=document.querySelector('.main-container');"
		<< "if(root){root.style.width=currentWidth+'px';root.style.height=currentHeight+'px';root.style.minWidth=currentWidth+'px';root.style.minHeight=currentHeight+'px';root.style.maxWidth=currentWidth+'px';root.style.maxHeight=currentHeight+'px';root.style.overflow='hidden';}"
		<< "if(body){body.style.width=currentWidth+'px';body.style.height=currentHeight+'px';body.style.minWidth=currentWidth+'px';body.style.minHeight=currentHeight+'px';body.style.maxWidth=currentWidth+'px';body.style.maxHeight=currentHeight+'px';body.style.margin='0';body.style.overflow='hidden';}"
		<< "if(mainContainer){mainContainer.style.width=currentWidth+'px';mainContainer.style.height=currentHeight+'px';mainContainer.style.minWidth=currentWidth+'px';mainContainer.style.minHeight=currentHeight+'px';mainContainer.style.maxWidth=currentWidth+'px';mainContainer.style.maxHeight=currentHeight+'px';mainContainer.style.boxSizing='border-box';mainContainer.dataset.currentWidth=String(currentWidth);mainContainer.dataset.currentHeight=String(currentHeight);mainContainer.dataset.referenceWidth=String(referenceWidth);mainContainer.dataset.referenceHeight=String(referenceHeight);mainContainer.dataset.scaleX=String(layoutScaleX);mainContainer.dataset.scaleY=String(layoutScaleY);}"
		<< "const left=resetBox(document.querySelector('.left-section'));"
		<< "const center=resetBox(document.querySelector('.center-section'));"
		<< "const right=resetBox(document.querySelector('.right-section'));"
		<< "const topRow=resetBox(document.querySelector('.top-row'));"
		<< "const leftPanel=clampPanel('" << leftPanelSelector << "');"
		<< "const rightTopPanel=" << (rightTopPanelSelector.empty() ? "null" : "clampPanel('" + rightTopPanelSelector + "')") << ";"
		<< "const rightBottomPanel=" << (rightBottomPanelSelector.empty() ? "null" : "clampPanel('" + rightBottomPanelSelector + "')") << ";"
		<< "const project=clampPanel('" << bottomPanelSelector << "');"
		<< buildFixedWidthBlock("left", leftWidth)
		<< "if(center){center.style.flex='1 1 auto';center.style.minWidth='0';center.style.overflow='hidden';}"
		<< buildFixedWidthBlock("right", rightWidth)
		<< buildFixedHeightBlock("topRow", topRowHeight)
		<< buildFixedPanelBlock("leftPanel", leftWidth, leftHeight)
		<< buildFixedPanelBlock("rightTopPanel", rightWidth, rightTopHeight)
		<< buildFixedPanelBlock("rightBottomPanel", rightWidth, rightBottomHeight)
		<< buildFixedHeightBlock("project", bottomHeight)
		<< "document.body.offsetHeight;"
		<< "window.requestAnimationFrame(function(){"
		<< "if(window.notifyViewportResize){window.notifyViewportResize();}"
		<< "if(isInitState){"
		<< "if(window.sendUIPanelIFramesToServer){window.sendUIPanelIFramesToServer();}"
		<< "if(window.sendIFrameSizesToServer){window.sendIFrameSizesToServer();}"
		<< "}"
		<< "document.body.offsetHeight;"
		<< "});"
		<< "})();";

	mainFrame->ExecuteJavaScript(script.str(), mainFrame->GetURL(), 0);
	browser->GetHost()->Invalidate(PET_VIEW);
	CefRefPtr<UIHandler> self(this);
	CefPostDelayedTask(
		TID_UI,
		base::BindOnce(&UIHandler::finalizeDeferredLayoutRefresh, self, browser),
		32);

	std::cout << "[UIHandler] syncBrowserPanelLayoutFromCurrentFrames: Applied splitter-adjusted layout to CEF DOM for "
		      << iframeDataMap.size() << " panels" << std::endl;
	}

void UIHandler::syncBrowserFullScreenPanelLayout(WindowRenderState windowState, int currentWidth, int currentHeight, int referenceWidth, int referenceHeight)
{
	if (!ui_manager_)
	{
		std::cout << "[UIHandler] syncBrowserFullScreenPanelLayout: skipped, ui_manager_ is null" << std::endl;
		return;
	}

	const auto iframeDataMap = ui_manager_->getResolvedUIPanelIFrames();
	if (iframeDataMap.empty())
	{
		std::cout << "[UIHandler] syncBrowserFullScreenPanelLayout: skipped, no resolved UI panel frames" << std::endl;
		return;
	}

	if (!CefCurrentlyOn(TID_UI))
	{
		CefPostTask(TID_UI, base::BindOnce(&UIHandler::syncBrowserFullScreenPanelLayout, base::Unretained(this), windowState, currentWidth, currentHeight, referenceWidth, referenceHeight));
		return;
	}

	if (!parent_sdl_window_)
	{
		std::cout << "[UIHandler] syncBrowserFullScreenPanelLayout: skipped, SDL parent window is null" << std::endl;
		return;
	}

	CefRefPtr<CefBrowser> browser = parent_sdl_window_->getBrowser();
	if (!browser)
	{
		std::cout << "[UIHandler] syncBrowserFullScreenPanelLayout: skipped, SDL browser is null" << std::endl;
		return;
	}

	CefRefPtr<CefFrame> mainFrame = browser->GetMainFrame();
	if (!mainFrame || !mainFrame->IsValid())
	{
		std::cout << "[UIHandler] syncBrowserFullScreenPanelLayout: skipped, main frame is invalid" << std::endl;
		return;
	}

	auto selectorForPanel = [](const std::string& panelName)
	{
		std::string selector = panelName;
		std::replace(selector.begin(), selector.end(), '_', '-');
		return selector;
	};

	const auto leftColumnIt = std::min_element(
		iframeDataMap.begin(), iframeDataMap.end(),
		[](const auto& lhs, const auto& rhs)
		{
			if (lhs.second.x != rhs.second.x)
			{
				return lhs.second.x < rhs.second.x;
			}
			return lhs.second.y < rhs.second.y;
		});

	const auto rightColumnIt = std::max_element(
		iframeDataMap.begin(), iframeDataMap.end(),
		[](const auto& lhs, const auto& rhs)
		{
			if (lhs.second.x != rhs.second.x)
			{
				return lhs.second.x < rhs.second.x;
			}
			return lhs.second.y < rhs.second.y;
		});

	const auto bottomPanelIt = std::max_element(
		iframeDataMap.begin(), iframeDataMap.end(),
		[](const auto& lhs, const auto& rhs)
		{
			if (lhs.second.width != rhs.second.width)
			{
				return lhs.second.width < rhs.second.width;
			}
			return lhs.second.x < rhs.second.x;
		});

	if (leftColumnIt == iframeDataMap.end() || rightColumnIt == iframeDataMap.end() || bottomPanelIt == iframeDataMap.end())
	{
		std::cout << "[UIHandler] syncBrowserFullScreenPanelLayout: skipped, unable to derive layout anchors" << std::endl;
		return;
	}

	int topRowHeight = 0;

	for (const auto& pair : iframeDataMap)
	{
		if (pair.first != bottomPanelIt->first)
		{
			topRowHeight = (std::max)(topRowHeight, pair.second.height);
		}
	}

	if (topRowHeight <= 0)
	{
		topRowHeight = leftColumnIt->second.height;
	}

	auto rightTopPanelIt = iframeDataMap.end();
	auto rightBottomPanelIt = iframeDataMap.end();

	for (auto it = iframeDataMap.begin(); it != iframeDataMap.end(); ++it)
	{
		if (it->first == bottomPanelIt->first || it->first == leftColumnIt->first)
			continue;

		if (it->second.x != rightColumnIt->second.x)
		{
			continue;
		}

		if (rightTopPanelIt == iframeDataMap.end() || it->second.y < rightTopPanelIt->second.y)
			rightTopPanelIt = it;

		if (rightBottomPanelIt == iframeDataMap.end() || it->second.y > rightBottomPanelIt->second.y)
			rightBottomPanelIt = it;
	}

	const std::string leftPanelSelector = selectorForPanel(leftColumnIt->first);
	const std::string bottomPanelSelector = selectorForPanel(bottomPanelIt->first);
	const std::string rightTopPanelSelector = (rightTopPanelIt != iframeDataMap.end()) ? selectorForPanel(rightTopPanelIt->first) : std::string();
	const std::string rightBottomPanelSelector = (rightBottomPanelIt != iframeDataMap.end()) ? selectorForPanel(rightBottomPanelIt->first) : std::string();

	const int fsLeftW = leftColumnIt->second.width;
	const int fsLeftH = leftColumnIt->second.height;
	const int fsRightW = rightColumnIt->second.width;
	const int fsTopRowH = topRowHeight;
	const int fsBottomH = bottomPanelIt->second.height;
	const int fsRightTopH = (rightTopPanelIt != iframeDataMap.end()) ? rightTopPanelIt->second.height : 0;
	const int fsRightBottomH = (rightBottomPanelIt != iframeDataMap.end()) ? rightBottomPanelIt->second.height : 0;

	std::cout << "[UIHandler][Fullscreen DOM] left=" << leftColumnIt->first
		      << " right=" << rightColumnIt->first
		      << " bottom=" << bottomPanelIt->first
		      << " rightTop=" << ((rightTopPanelIt != iframeDataMap.end()) ? rightTopPanelIt->first : std::string("<none>"))
		      << " rightBottom=" << ((rightBottomPanelIt != iframeDataMap.end()) ? rightBottomPanelIt->first : std::string("<none>"))
		      << std::endl;

	std::ostringstream script;
	script
		<< "(function(){"
		<< "const currentWidth=" << currentWidth << ";"
		<< "const currentHeight=" << currentHeight << ";"
		<< "const findPanelElement=function(selector){"
		<< "return document.querySelector('.'+selector) || document.querySelector('.'+selector.replace(/-UI/g,'_UI'));"
		<< "};"
		<< "const resetBox=function(element){"
		<< "if(!element){return null;}"
		<< "element.style.width='';"
		<< "element.style.height='';"
		<< "element.style.flex='';"
		<< "element.style.minWidth='';"
		<< "element.style.minHeight='';"
		<< "element.style.maxWidth='';"
		<< "element.style.maxHeight='';"
		<< "element.style.alignSelf='';"
		<< "return element;"
		<< "};"
		<< "const clampPanel=function(selector){"
		<< "const element=resetBox(findPanelElement(selector));"
		<< "if(!element){return null;}"
		<< "element.style.overflow='hidden';"
		<< "element.style.margin='0';"
		<< "element.style.boxSizing='border-box';"
		<< "return element;"
		<< "};"
		<< "const root=document.documentElement;"
		<< "const body=document.body;"
		<< "const mainContainer=document.querySelector('.main-container');"
		<< "if(root){root.style.width=currentWidth+'px';root.style.height=currentHeight+'px';root.style.minWidth=currentWidth+'px';root.style.minHeight=currentHeight+'px';root.style.maxWidth=currentWidth+'px';root.style.maxHeight=currentHeight+'px';root.style.overflow='hidden';}"
		<< "if(body){body.style.width=currentWidth+'px';body.style.height=currentHeight+'px';body.style.minWidth=currentWidth+'px';body.style.minHeight=currentHeight+'px';body.style.maxWidth=currentWidth+'px';body.style.maxHeight=currentHeight+'px';body.style.margin='0';body.style.overflow='hidden';}"
		<< "if(mainContainer){mainContainer.style.width=currentWidth+'px';mainContainer.style.height=currentHeight+'px';mainContainer.style.minWidth=currentWidth+'px';mainContainer.style.minHeight=currentHeight+'px';mainContainer.style.maxWidth=currentWidth+'px';mainContainer.style.maxHeight=currentHeight+'px';mainContainer.style.boxSizing='border-box';}"
		<< "const left=resetBox(document.querySelector('.left-section'));"
		<< "const center=resetBox(document.querySelector('.center-section'));"
		<< "const right=resetBox(document.querySelector('.right-section'));"
		<< "const topRow=resetBox(document.querySelector('.top-row'));"
		<< "const leftPanel=clampPanel('" << leftPanelSelector << "');"
		<< "const rightTopPanel=" << (rightTopPanelSelector.empty() ? "null" : "clampPanel('" + rightTopPanelSelector + "')") << ";"
		<< "const rightBottomPanel=" << (rightBottomPanelSelector.empty() ? "null" : "clampPanel('" + rightBottomPanelSelector + "')") << ";"
		<< "const project=clampPanel('" << bottomPanelSelector << "');"
		<< "if(left){left.style.width='" << fsLeftW << "px';left.style.minWidth='" << fsLeftW << "px';left.style.maxWidth='" << fsLeftW << "px';left.style.flex='0 0 " << fsLeftW << "px';left.style.overflow='hidden';}"
		<< "if(center){center.style.flex='1 1 auto';center.style.minWidth='0';center.style.overflow='hidden';}"
		<< "if(right){right.style.width='" << fsRightW << "px';right.style.minWidth='" << fsRightW << "px';right.style.maxWidth='" << fsRightW << "px';right.style.flex='0 0 " << fsRightW << "px';right.style.overflow='hidden';}"
		<< "if(topRow){topRow.style.height='" << fsTopRowH << "px';topRow.style.minHeight='" << fsTopRowH << "px';topRow.style.maxHeight='" << fsTopRowH << "px';topRow.style.flex='0 0 " << fsTopRowH << "px';topRow.style.overflow='hidden';}"
		<< "if(leftPanel){leftPanel.style.width='" << fsLeftW << "px';leftPanel.style.height='" << fsLeftH << "px';leftPanel.style.minWidth='" << fsLeftW << "px';leftPanel.style.minHeight='" << fsLeftH << "px';leftPanel.style.maxWidth='" << fsLeftW << "px';leftPanel.style.maxHeight='" << fsLeftH << "px';leftPanel.style.flex='0 0 auto';}"
		<< "if(rightTopPanel){rightTopPanel.style.width='" << fsRightW << "px';rightTopPanel.style.height='" << fsRightTopH << "px';rightTopPanel.style.minWidth='" << fsRightW << "px';rightTopPanel.style.minHeight='" << fsRightTopH << "px';rightTopPanel.style.maxWidth='" << fsRightW << "px';rightTopPanel.style.maxHeight='" << fsRightTopH << "px';rightTopPanel.style.flex='0 0 auto';}"
		<< "if(rightBottomPanel){rightBottomPanel.style.width='" << fsRightW << "px';rightBottomPanel.style.height='" << fsRightBottomH << "px';rightBottomPanel.style.minWidth='" << fsRightW << "px';rightBottomPanel.style.minHeight='" << fsRightBottomH << "px';rightBottomPanel.style.maxWidth='" << fsRightW << "px';rightBottomPanel.style.maxHeight='" << fsRightBottomH << "px';rightBottomPanel.style.flex='0 0 auto';}"
		<< "if(project){project.style.width='100%';project.style.minWidth='0';project.style.maxWidth='100%';project.style.alignSelf='stretch';project.style.height='" << fsBottomH << "px';project.style.minHeight='" << fsBottomH << "px';project.style.maxHeight='" << fsBottomH << "px';project.style.flex='0 0 " << fsBottomH << "px';}"
		<< "})();";

	mainFrame->ExecuteJavaScript(script.str(), mainFrame->GetURL(), 0);
	browser->GetHost()->Invalidate(PET_VIEW);
	CefRefPtr<UIHandler> self(this);
	CefPostDelayedTask(
		TID_UI,
		base::BindOnce(&UIHandler::finalizeDeferredLayoutRefresh, self, browser),
		32);

	std::cout << "[UIHandler] syncBrowserFullScreenPanelLayout: Applied fullscreen layout to CEF DOM for "
		      << iframeDataMap.size() << " panels" << std::endl;
}

void UIHandler::clearUIPanels()
{

	splitter_.reset();
	iframe_data_map_.clear();
	pending_iframe_capture_.store(false);
	pending_ui_panel_cache_.store(false);
	pending_deferred_layout_refresh_.store(false);
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