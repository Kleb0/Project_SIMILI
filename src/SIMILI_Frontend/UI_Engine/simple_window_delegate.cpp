#include "simple_window_delegate.hpp"
#include "ui_handler.hpp"

SimpleWindowDelegate::SimpleWindowDelegate(CefRefPtr<CefBrowserView> browser_view)
	: browser_view_(browser_view) {
}

void SimpleWindowDelegate::OnWindowCreated(CefRefPtr<CefWindow> window) {
	window->AddChildView(browser_view_);
	
	window->SetTitle("SIMILI - Object List");
	
	window->Show();
	
	browser_view_->RequestFocus();
	
	HWND window_hwnd = window->GetWindowHandle();
	
	if (window_hwnd) 
	{
		CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
		if (browser) 
		{
			HWND browser_hwnd = browser->GetHost()->GetWindowHandle();
			
			CefRefPtr<CefClient> client = browser->GetHost()->GetClient();
			UIHandler* handler = static_cast<UIHandler*>(client.get());
			if (handler) 
			{
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
