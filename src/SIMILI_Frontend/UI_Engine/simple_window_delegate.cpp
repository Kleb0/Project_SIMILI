#include "simple_window_delegate.hpp"
#include "ui_handler.hpp"

SimpleWindowDelegate::SimpleWindowDelegate(CefRefPtr<CefBrowserView> browser_view)
	: browser_view_(browser_view), window_hwnd_(nullptr), ui_handler_(nullptr), maximization_captured_(false) {
}

void SimpleWindowDelegate::OnWindowCreated(CefRefPtr<CefWindow> window) {
	window->AddChildView(browser_view_);
	
	window->SetTitle("SIMLI PROJECT");
	
	window->Show();
	
	browser_view_->RequestFocus();
	
	window_hwnd_ = window->GetWindowHandle();
	
	if (window_hwnd_) 
	{
		CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
		if (browser) 
		{
			HWND browser_hwnd = browser->GetHost()->GetWindowHandle();
			
			CefRefPtr<CefClient> client = browser->GetHost()->GetClient();
			UIHandler* handler = static_cast<UIHandler*>(client.get());
			if (handler) 
			{				ui_handler_ = handler;				handler->createOverlayViewport(browser_hwnd);
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
