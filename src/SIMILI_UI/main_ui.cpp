#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_sandbox_win.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "ui_app.hpp"
#include "ui_handler.hpp"
#include "simple_window_delegate.hpp"
#include "simple_browser_view_delegate.hpp"

#ifdef _WIN32
#include <windows.h>

int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPTSTR lpCmdLine,
                      int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    CefMainArgs main_args(hInstance);
    
    CefRefPtr<UIApp> app(new UIApp);

    int exit_code = CefExecuteProcess(main_args, app, nullptr);
    if (exit_code >= 0) {
        return exit_code;
    }

    CefSettings settings;
    settings.no_sandbox = true;
    settings.multi_threaded_message_loop = false;
    
    CefString(&settings.log_file).FromASCII("simili_ui.log");
    settings.log_severity = LOGSEVERITY_WARNING;

    CefInitialize(main_args, settings, app, nullptr);

    CefRefPtr<UIHandler> handler(new UIHandler);

    CefBrowserSettings browser_settings;
    browser_settings.windowless_frame_rate = 60;

    std::string url = "file:///e:/Project_SIMILI/Project/ui/object_list.html";

    CefRefPtr<SimpleBrowserViewDelegate> browser_view_delegate(new SimpleBrowserViewDelegate());
    
    CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
        handler, url, browser_settings, nullptr, nullptr, browser_view_delegate);

    CefRefPtr<SimpleWindowDelegate> window_delegate(new SimpleWindowDelegate(browser_view));
    
    CefWindow::CreateTopLevelWindow(window_delegate);

    CefRunMessageLoop();

    CefShutdown();

    return 0;
}
#endif 
