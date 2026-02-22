#include "simple_window_delegate.hpp"
#include "ui_handler.hpp"
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

SimpleWindowDelegate::SimpleWindowDelegate(CefRefPtr<CefBrowserView> browser_view)
	: browser_view_(browser_view), window_hwnd_(nullptr), ui_handler_(nullptr), maximization_captured_(false),
	  last_window_x_(0), last_window_y_(0) {
}

void SimpleWindowDelegate::OnWindowCreated(CefRefPtr<CefWindow> window) {
	window->AddChildView(browser_view_);
	
	window->SetTitle("SIMLI PROJECT");
	
	window->Show();
	
	browser_view_->RequestFocus();
	
	window_hwnd_ = window->GetWindowHandle();
	
	if (window_hwnd_) 
	{
		// Initialize last window position
		RECT windowRect;
		if (GetWindowRect(window_hwnd_, &windowRect))
		{
			last_window_x_ = windowRect.left;
			last_window_y_ = windowRect.top;
		}
		
		// Install subclass to monitor window movement
		SetWindowSubclass(window_hwnd_, WindowSubclassProc, 1, reinterpret_cast<DWORD_PTR>(this));
		
		CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
		if (browser) 
		{
			HWND browser_hwnd = browser->GetHost()->GetWindowHandle();
			
			CefRefPtr<CefClient> client = browser->GetHost()->GetClient();
			UIHandler* handler = static_cast<UIHandler*>(client.get());

			if (handler) 
			{	
				ui_handler_ = handler;				
				handler->createOverlayViewport(browser_hwnd);
			} 
			else 
			{
				std::cout << "[SimpleWindowDelegate] Handler is NULL!" << std::endl;
			}
		} 
		else 
		{
			std::cout << "[SimpleWindowDelegate] Browser is NULL!" << std::endl;
		}
	} 
	else 
	{
		std::cout << "[SimpleWindowDelegate] HWND is NULL!" << std::endl;
	}
}

void SimpleWindowDelegate::OnWindowDestroyed(CefRefPtr<CefWindow> window) {
	if (window_hwnd_)
	{
		// Remove subclass before window destruction
		RemoveWindowSubclass(window_hwnd_, WindowSubclassProc, 1);
		std::cout << "[SimpleWindowDelegate] Window subclass removed" << std::endl;
	}
	browser_view_ = nullptr;
}

bool SimpleWindowDelegate::CanClose(CefRefPtr<CefWindow> window) {
	CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
	if (browser) {
		browser->GetHost()->CloseBrowser(false);
	}
	return true;
}

CefSize SimpleWindowDelegate::GetPreferredSize(CefRefPtr<CefView> view) {
	return CefSize(800, 600);
}

bool SimpleWindowDelegate::isWindowMaximized() const
{
    if (!window_hwnd_) {
        return false;
    }
    
    WINDOWPLACEMENT placement;
    placement.length = sizeof(WINDOWPLACEMENT);
    
    if (GetWindowPlacement(window_hwnd_, &placement)) {
        return placement.showCmd == SW_SHOWMAXIMIZED;
    }
    
    return false;
}

void SimpleWindowDelegate::getMaximizedBorderOffsets(int& offsetX, int& offsetY, int& offsetWidth, int& offsetHeight) const
{
    offsetX = 0;
    offsetY = 0;
    offsetWidth = 0;
    offsetHeight = 0;
    
    if (!window_hwnd_ || !isWindowMaximized()) {
        return;
    }
    
    // When maximized, Windows adds invisible borders
    // Get the window frame size
    RECT windowRect, clientRect;
    GetWindowRect(window_hwnd_, &windowRect);
    GetClientRect(window_hwnd_, &clientRect);
    
    POINT clientOrigin = {0, 0};
    ClientToScreen(window_hwnd_, &clientOrigin);
    
    // Calculate the offset (invisible border size)
    offsetX = clientOrigin.x - windowRect.left;
    offsetY = clientOrigin.y - windowRect.top;
    
    // Width and height offsets (borders on both sides)
    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;
    int clientWidth = clientRect.right - clientRect.left;
    int clientHeight = clientRect.bottom - clientRect.top;
    
    offsetWidth = windowWidth - clientWidth;
    offsetHeight = windowHeight - clientHeight;
    
    std::cout << "[SimpleWindowDelegate] Maximized border offsets: X=" << offsetX 
              << " Y=" << offsetY 
              << " W=" << offsetWidth 
              << " H=" << offsetHeight << std::endl;
}

void SimpleWindowDelegate::checkAndCaptureIfMaximized()
{
	if (maximization_captured_ || !window_hwnd_ || !ui_handler_)
	{
		return;
	}
	
	if (isWindowMaximized())
	{
		std::cout << "\n[SimpleWindowDelegate] Window maximized detected - recapturing iframe positions..." << std::endl;
		maximization_captured_ = true;
		ui_handler_->captureIFramePositions();
	}
}

LRESULT CALLBACK SimpleWindowDelegate::WindowSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	SimpleWindowDelegate* delegate = reinterpret_cast<SimpleWindowDelegate*>(dwRefData);
	
	switch (msg)
	{
		case WM_EXITSIZEMOVE:
		{
			// Called ONLY when user finishes moving or resizing the window
			if (delegate && delegate->ui_handler_)
			{
				RECT windowRect;
				if (GetWindowRect(hwnd, &windowRect))
				{
					int newX = windowRect.left;
					int newY = windowRect.top;
					
					// Check if window actually moved (avoid redundant captures)
					if (newX != delegate->last_window_x_ || newY != delegate->last_window_y_)
					{
						delegate->last_window_x_ = newX;
						delegate->last_window_y_ = newY;
						
						std::cout << "[SimpleWindowDelegate] Window moved to (" << newX << ", " << newY 
						          << ") - recapturing iframe positions..." << std::endl;
						
						delegate->ui_handler_->captureIFramePositions();
					}
				}
			}
			break;
		}
		
		case WM_WINDOWPOSCHANGED:
		{
			// Handle window state changes (maximize/restore)
			if (delegate && delegate->ui_handler_)
			{
				WINDOWPOS* pos = reinterpret_cast<WINDOWPOS*>(lParam);
				
				// Check if position changed (not just size)
				if (pos && !(pos->flags & SWP_NOMOVE))
				{
					delegate->checkAndCaptureIfMaximized();
				}
			}
			break;
		}
	}
	
	return DefSubclassProc(hwnd, msg, wParam, lParam);
}

void SimpleWindowDelegate::updateIFrameData(const std::string& name, int x, int y, int width, int height, int clientX, int clientY)
{
	std::lock_guard<std::mutex> lock(iframe_mutex_);
	
	IFrameData data;
	data.name = name;
	data.x = x;
	data.y = y;
	data.width = width;
	data.height = height;
	data.clientX = clientX;
	data.clientY = clientY;
	
	iframeDataMap_[name] = data;
}

bool SimpleWindowDelegate::getIFrameData(const std::string& name, IFrameData& outData) const
{
	std::lock_guard<std::mutex> lock(iframe_mutex_);
	
	auto it = iframeDataMap_.find(name);
	if (it != iframeDataMap_.end())
	{
		outData = it->second;
		return true;
	}
	
	return false;
}

std::map<std::string, IFrameData> SimpleWindowDelegate::getAllIFrames() const
{
	std::lock_guard<std::mutex> lock(iframe_mutex_);
	return iframeDataMap_;
}

void SimpleWindowDelegate::clearAllIFrames()
{
	std::lock_guard<std::mutex> lock(iframe_mutex_);
	iframeDataMap_.clear();
}
