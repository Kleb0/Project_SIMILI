#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_sandbox_win.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "ui_app.hpp"
#include "ui_handler.hpp"
#include "simple_window_delegate.hpp"
#include "simple_browser_view_delegate.hpp"
#include "../../SIMILI_Services/router/RouterSim.hpp"
#include "../../SIMILI_Services/types/RouterTypes.hpp"
#include "../../SIMILI_Services/middleware/SimpleHttpServer.hpp"
#include <iostream>

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

    static bool console_allocated = false;
    if (!console_allocated) {
        AllocConsole();
        FILE* fp;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        console_allocated = true;
        std::cout << "[Main] Console allocated for debugging" << std::endl;
    }

    // Start HTTP/HTTPS Server
    SIMILI::Server::SimpleHttpServer http_server;
    http_server.start(8080, 8443);

    SIMILI::Router::RouterSim router;
    router.setDebugMode(true);
    
    router.get("/ping", [](const SIMILI::Router::Message& msg) -> SIMILI::Router::Response {
        SIMILI::Router::Response response(msg.requestId);
        response.body = R"({"message":"pong from UI"})";
        return response;
    }, "Connectivity test from UI");

    std::cout << "[Main] Router SIMILI Initialized" << std::endl;

    CefSettings settings;
    settings.no_sandbox = true;
    settings.multi_threaded_message_loop = false;
    
    // Disable logging to avoid errors
    settings.log_severity = LOGSEVERITY_DISABLE;

    if (!CefInitialize(main_args, settings, app, nullptr)) {
        std::cerr << "[Main] Failed to initialize CEF" << std::endl;
        return -1;
    }

    CefRefPtr<UIHandler> handler(new UIHandler);

    CefBrowserSettings browser_settings;
    browser_settings.windowless_frame_rate = 60;

    std::string url = "file:///ui/main_layout.html";

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
 
