#include "ui_handler.hpp"
#include <iostream>
#include <chrono>
#include <commctrl.h>  

#pragma comment(lib, "comctl32.lib")

UIHandler::UIHandler() : ipc_client_("SIMILI_IPC"), ipc_running_(false), parent_hwnd_(nullptr), timer_id_(0)
{
	CefMessageRouterConfig config;
	message_router_ = CefMessageRouterBrowserSide::Create(config);
	
	message_handler_ = new MessageHandler(&ipc_client_);
	message_router_->AddHandler(message_handler_, false);
}

UIHandler::~UIHandler() 
{
	stopRenderTimer();
	stopIPCListener();
	message_router_->RemoveHandler(message_handler_);
	delete message_handler_;
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

CefRefPtr<CefRequestHandler> UIHandler::GetRequestHandler() 
{
	return this;
}

void UIHandler::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) 
{
}

void UIHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) 
{
	CEF_REQUIRE_UI_THREAD();

	browser_list_.push_back(browser);
	
	ipc_client_.send("browser_created");
	
	startIPCListener(browser);
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
		if ((*it)->IsSame(browser)) {
			browser_list_.erase(it);
			break;
		}
	}

	if (browser_list_.empty()) 
	{
		ipc_client_.send("browser_closed");
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

bool UIHandler::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
CefRefPtr<CefFrame> frame, CefProcessId source_process, CefRefPtr<CefProcessMessage> message) 
{
	CEF_REQUIRE_UI_THREAD();
	return message_router_->OnProcessMessageReceived(browser, frame, source_process, message);
}

void UIHandler::CloseAllBrowsers(bool force_close) 
{
	if (!CefCurrentlyOn(TID_UI)) {
		return;
	}

	for (auto it = browser_list_.begin(); it != browser_list_.end(); ++it) 
	{
		(*it)->GetHost()->CloseBrowser(force_close);
	}
}

void UIHandler::startIPCListener(CefRefPtr<CefBrowser> browser) 
{
	if (ipc_running_) return;
	
	std::cout << "[UIHandler] Starting IPC listener thread..." << std::endl;
	ipc_running_ = true;
	ipc_thread_ = std::thread([this, browser]() 
	{
		std::cout << "[UIHandler] IPC listener thread started" << std::endl;
		while (ipc_running_) 
		{
			std::string message = ipc_client_.receive();

			if (!message.empty() && browser && browser->GetMainFrame()) 
			{
				std::cout << "[UIHandler] Received IPC message, executing JS..." << std::endl;
				std::string js = "if (window.handleIPCMessage) { window.handleIPCMessage(" + message + "); }";
				browser->GetMainFrame()->ExecuteJavaScript(js, browser->GetMainFrame()->GetURL(), 0);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		std::cout << "[UIHandler] IPC listener thread stopped" << std::endl;
	});
}

void UIHandler::stopIPCListener() 
{
	if (ipc_running_) 
	{
		ipc_running_ = false;
		if (ipc_thread_.joinable()) 
		{
			ipc_thread_.join();
		}
	}
}

void UIHandler::createOverlayViewport(HWND parent_hwnd) 
{
	parent_hwnd_ = parent_hwnd;
	
	std::cout << "[UIHandler] Creating overlay with parent HWND: " << parent_hwnd << std::endl;
	
	if (!overlay_viewport_) {
		overlay_viewport_ = std::make_unique<OverlayViewport>();
	}
	
	// Calculate viewport position (center area of the layout)
	// These values should match the CSS grid layout
	// For now, using estimated values - will be refined
	RECT client_rect;
	GetClientRect(parent_hwnd, &client_rect);
	
	int window_width = client_rect.right - client_rect.left;
	int window_height = client_rect.bottom - client_rect.top;
	
	std::cout << "[UIHandler] Parent window size: " << window_width << "x" << window_height << std::endl;
	
	// Grid: 15% | 1fr | 15%, Rows: 60% | 40%
	int left_panel_width = (int)(window_width * 0.15f);
	int right_panel_width = (int)(window_width * 0.15f);
	int viewport_width = window_width - left_panel_width - right_panel_width;
	int viewport_height = (int)(window_height * 0.60f);
	
	std::cout << "[UIHandler] Overlay position: (" << left_panel_width << ", 0) size: " 
	          << viewport_width << "x" << viewport_height << std::endl;
	
	overlay_viewport_->create(parent_hwnd, left_panel_width, 0, viewport_width, viewport_height);
	overlay_viewport_->show(true);
	
	std::cout << "[UIHandler] Overlay viewport created" << std::endl;
	
	// Install window subclass to handle resize
	SetWindowSubclass(parent_hwnd, ParentWindowProc, 0, reinterpret_cast<DWORD_PTR>(this));
	
	// Start render timer (60 FPS)
	startRenderTimer();
}

void UIHandler::updateOverlayPosition() 
{
	if (overlay_viewport_ && parent_hwnd_) {
		RECT client_rect;
		GetClientRect(parent_hwnd_, &client_rect);
		
		int window_width = client_rect.right - client_rect.left;
		int window_height = client_rect.bottom - client_rect.top;
		
		int left_panel_width = (int)(window_width * 0.15f);
		int right_panel_width = (int)(window_width * 0.15f);
		int viewport_width = window_width - left_panel_width - right_panel_width;
		int viewport_height = (int)(window_height * 0.60f);
		
		overlay_viewport_->setPosition(left_panel_width, 0, viewport_width, viewport_height);
	}
}

static VOID CALLBACK RenderTimerProc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) {
	// Timer callback - trigger redraw
	UIHandler* handler = reinterpret_cast<UIHandler*>(idEvent);
	if (handler && handler->getOverlay()) {
		InvalidateRect(handler->getOverlay()->getHandle(), nullptr, FALSE);
	}
}

void UIHandler::startRenderTimer() {
	if (timer_id_ == 0 && overlay_viewport_) {
		// 60 FPS = ~16ms
		timer_id_ = SetTimer(nullptr, reinterpret_cast<UINT_PTR>(this), 16, RenderTimerProc);
		std::cout << "[UIHandler] Render timer started (60 FPS)" << std::endl;
	}
}

void UIHandler::stopRenderTimer() {
	if (timer_id_ != 0) {
		KillTimer(nullptr, timer_id_);
		timer_id_ = 0;
		std::cout << "[UIHandler] Render timer stopped" << std::endl;
	}
}

LRESULT CALLBACK UIHandler::ParentWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) 
{
	UIHandler* handler = reinterpret_cast<UIHandler*>(dwRefData);
	
	switch (msg) {
		case WM_SIZE:
			if (handler && handler->overlay_viewport_) {
				handler->updateOverlayPosition();
			}
			break;
			
		case WM_NCDESTROY:
			// Remove subclass before window is destroyed
			RemoveWindowSubclass(hwnd, ParentWindowProc, uIdSubclass);
			break;
	}
	
	return DefSubclassProc(hwnd, msg, wParam, lParam);
}
