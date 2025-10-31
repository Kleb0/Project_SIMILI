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
    
    HWND hwnd = window->GetWindowHandle();
    std::cout << "[SimpleWindowDelegate] Window HWND: " << hwnd << std::endl;
    
    if (hwnd) {
        CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
        if (browser) {
            CefRefPtr<CefClient> client = browser->GetHost()->GetClient();
            UIHandler* handler = static_cast<UIHandler*>(client.get());
            if (handler) {
                std::cout << "[SimpleWindowDelegate] Creating overlay viewport..." << std::endl;
                handler->createOverlayViewport(hwnd);
                std::cout << "[SimpleWindowDelegate] Overlay created!" << std::endl;
                
                handler->stopRenderTimer();
                handler->enableOverlayRendering(false);
                
                if (handler->getOverlay()) {
                    handler->getOverlay()->show(false);
                }
                
                std::cout << "[SimpleWindowDelegate] OpenGL rendering STOPPED and overlay HIDDEN" << std::endl;
            } else {
                std::cout << "[SimpleWindowDelegate] Handler is NULL!" << std::endl;
            }
        } else {
            std::cout << "[SimpleWindowDelegate] Browser is NULL!" << std::endl;
        }
    } else {
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
