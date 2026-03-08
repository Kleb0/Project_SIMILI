#include "simple_window_delegate.hpp"
#include "ui_handler.hpp"
#include <SDL3/SDL.h>

SimpleWindowDelegate::SimpleWindowDelegate(CefRefPtr<CefBrowserView> browser_view)
	: browser_view_(browser_view), cef_window_(nullptr), sdl_window_(nullptr), ui_handler_(nullptr), 
	  last_maximized_state_(false), last_window_x_(0), last_window_y_(0) {
}

void SimpleWindowDelegate::OnWindowCreated(CefRefPtr<CefWindow> window) 
{
	window->AddChildView(browser_view_);
	
	window->SetTitle("SIMLI PROJECT");
	
	window->Show();
	
	browser_view_->RequestFocus();
	
	// Store CEF window reference
	cef_window_ = window;
	
	// Create an independent SDL3 window for the overlay viewport
	// We'll create this as a separate window that can be positioned independently
	sdl_window_ = SDL_CreateWindow(
		"SIMILI Overlay",
		800, 600,
		SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP
	);
	
	if (sdl_window_) 
	{
		// Set initial window position
		SDL_SetWindowPosition(sdl_window_, 100, 100);
		
		// Initialize last window position from SDL window
		SDL_GetWindowPosition(sdl_window_, &last_window_x_, &last_window_y_);
		
		CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
		if (browser) 
		{
			CefRefPtr<CefClient> client = browser->GetHost()->GetClient();
			UIHandler* handler = static_cast<UIHandler*>(client.get());

			if (handler) 
			{	
				ui_handler_ = handler;
				handler->createOverlayViewport(sdl_window_);
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
		std::cout << "[SimpleWindowDelegate] SDL_Window creation failed: " << SDL_GetError() << std::endl;
	}
}

void SimpleWindowDelegate::OnWindowDestroyed(CefRefPtr<CefWindow> window) {
	if (sdl_window_)
	{
		SDL_DestroyWindow(sdl_window_);
		sdl_window_ = nullptr;
		std::cout << "[SimpleWindowDelegate] SDL window destroyed" << std::endl;
	}
	cef_window_ = nullptr;
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
    if (!sdl_window_) {
        return false;
    }
    
    Uint32 flags = SDL_GetWindowFlags(sdl_window_);
    return (flags & SDL_WINDOW_MAXIMIZED) != 0;
}

void SimpleWindowDelegate::getMaximizedBorderOffsets(int& offsetX, int& offsetY, int& offsetWidth, int& offsetHeight) const
{
    offsetX = 0;
    offsetY = 0;
    offsetWidth = 0;
    offsetHeight = 0;
    
    if (!sdl_window_ || !isWindowMaximized()) {
        return;
    }
    
    // When maximized on Windows, SDL windows may have invisible borders
    // Get the window size vs client size using SDL3
    int windowWidth, windowHeight;
    SDL_GetWindowSize(sdl_window_, &windowWidth, &windowHeight);
    
    int windowX, windowY;
    SDL_GetWindowPosition(sdl_window_, &windowX, &windowY);
    
    // In SDL3, the decorated window size already accounts for borders
    // For a borderless window, these should be zero, but we calculate just in case
    SDL_Rect borderSize;
    if (SDL_GetWindowBordersSize(sdl_window_, &borderSize.y, &borderSize.x, &borderSize.h, &borderSize.w) == 0)
    {
        offsetX = borderSize.x;
        offsetY = borderSize.y;
        offsetWidth = borderSize.x + borderSize.w;
        offsetHeight = borderSize.y + borderSize.h;
        
        std::cout << "[SimpleWindowDelegate] Maximized border offsets: X=" << offsetX 
                  << " Y=" << offsetY 
                  << " W=" << offsetWidth 
                  << " H=" << offsetHeight << std::endl;
    }
}

void SimpleWindowDelegate::checkAndCaptureWindowStateChange()
{
	if (!sdl_window_ || !ui_handler_)
	{
		return;
	}
	
	bool current_maximized = isWindowMaximized();
	
	// Detect state change (maximize or restore)
	if (current_maximized != last_maximized_state_)
	{
		last_maximized_state_ = current_maximized;
		
		if (current_maximized)
		{
			std::cout << "\n[SimpleWindowDelegate] Window MAXIMIZED - repositioning panel..." << std::endl;
		}
		else
		{
			std::cout << "\n[SimpleWindowDelegate] Window RESTORED - repositioning panel..." << std::endl;
		}
		
		// Request panel repositioning before capturing positions
		if (auto renderer = ui_handler_->getCompositeTestRenderer())
		{
			renderer->RequestPanelPositionUpdate(PanelAnchorPosition::CurrentAnchorState);
		}
		
		ui_handler_->captureIFramePositions();
	}
}

void SimpleWindowDelegate::pollWindowEvents()
{
	if (!sdl_window_ || !ui_handler_)
	{
		return;
	}
	
	// Check for window position changes
	int currentX, currentY;
	SDL_GetWindowPosition(sdl_window_, &currentX, &currentY);
	
	if (currentX != last_window_x_ || currentY != last_window_y_)
	{
		last_window_x_ = currentX;
		last_window_y_ = currentY;
		
		std::cout << "[SimpleWindowDelegate] Window moved to (" << currentX << ", " << currentY 
		          << ") - recapturing iframe positions..." << std::endl;
		
		// Request panel repositioning before capturing positions
		if (auto renderer = ui_handler_->getCompositeTestRenderer())
		{
			renderer->RequestPanelPositionUpdate(PanelAnchorPosition::CurrentAnchorState);
		}
		
		ui_handler_->captureIFramePositions();
	}
	
	// Check for window state changes (maximize/restore)
	checkAndCaptureWindowStateChange();
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
