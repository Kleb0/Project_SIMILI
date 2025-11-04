#include "ui_handler.hpp"
#include <iostream>
#include <commctrl.h>  

#pragma comment(lib, "comctl32.lib")

UIHandler::UIHandler() : parent_hwnd_(nullptr), timer_id_(0),
	last_viewport_update_time_(0), last_viewport_x_(0), last_viewport_y_(0), 
	last_viewport_width_(0), last_viewport_height_(0)
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
				
				if (position_changed || time_elapsed || !overlay_viewport_->isVisible()) {
					overlay_viewport_->setPosition(final_x, final_y, final_width, final_height);
					
					last_viewport_update_time_ = current_time;
					last_viewport_x_ = final_x;
					last_viewport_y_ = final_y;
					last_viewport_width_ = final_width;
					last_viewport_height_ = final_height;
					
					if (!overlay_viewport_->isVisible()) {
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
		if ((*it)->IsSame(browser)) {
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
	if (!CefCurrentlyOn(TID_UI)) {
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
	
	std::cout << "[UIHandler] Creating overlay with parent HWND: " << parent_hwnd << std::endl;
	
	if (!overlay_viewport_) 
	{
		overlay_viewport_ = std::make_unique<OverlayViewport>();
	}

	RECT client_rect;
	GetClientRect(parent_hwnd, &client_rect);
	
	int window_width = client_rect.right - client_rect.left;
	int window_height = client_rect.bottom - client_rect.top;
	
	std::cout << "[UIHandler] Parent window size: " << window_width << "x" << window_height << std::endl;
	
	// Create overlay with placeholder dimensions (will be updated by JavaScript)
	int overlay_x = 0;
	int overlay_y = 0;
	int overlay_w = 100;
	int overlay_h = 100;
	
	overlay_viewport_->create(parent_hwnd, overlay_x, overlay_y, overlay_w, overlay_h);
	// Hide overlay until JavaScript sends proper dimensions
	overlay_viewport_->show(false);
	
	std::cout << "[UIHandler] Overlay created but hidden - waiting for JavaScript dimensions" << std::endl;
	
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

void UIHandler::enableOverlayRendering(bool enable) {
	if (overlay_viewport_) {
		overlay_viewport_->enableRendering(enable);
		std::cout << "[UIHandler] Overlay rendering " << (enable ? "enabled" : "disabled") << std::endl;
	}
}

bool UIHandler::isOverlayRenderingEnabled() const {
	if (overlay_viewport_) {
		return overlay_viewport_->isRenderingEnabled();
	}
	return false;
}

LRESULT CALLBACK UIHandler::ParentWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) 
{
	UIHandler* handler = reinterpret_cast<UIHandler*>(dwRefData);
	
	switch (msg) {
		case WM_SIZE:
			// Don't call updateOverlayPosition() here - JavaScript will send new dimensions
			// via layout_resizer.js when window resizes
			if (handler && handler->overlay_viewport_) {
				// Just ensure overlay stays on top
				SetWindowPos(handler->overlay_viewport_->getHandle(), HWND_TOP, 0, 0, 0, 0,
				             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
			}
			break;
		
		case WM_ACTIVATE:
		case WM_WINDOWPOSCHANGED:
			if (handler && handler->overlay_viewport_) {
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
