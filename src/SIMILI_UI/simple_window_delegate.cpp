#include "simple_window_delegate.hpp"

SimpleWindowDelegate::SimpleWindowDelegate(CefRefPtr<CefBrowserView> browser_view)
    : browser_view_(browser_view) {
}

void SimpleWindowDelegate::OnWindowCreated(CefRefPtr<CefWindow> window) {
    window->AddChildView(browser_view_);
    
    window->SetTitle("SIMILI - Object List");
    
    window->Show();
    
    browser_view_->RequestFocus();
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
