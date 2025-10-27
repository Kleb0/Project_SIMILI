// main_ui.cpp - Point d'entrée du processus CEF
#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_sandbox_win.h"
#include "ui_app.hpp"
#include "ui_handler.hpp"

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

    CefWindowInfo window_info;
    window_info.SetAsPopup(nullptr, "Hello CEF - SIMILI");

    CefBrowserSettings browser_settings;

    CefRefPtr<UIHandler> handler(new UIHandler);

    std::string url = "file:///e:/Project_SIMILI/Project/ui/hello_cef.html";

    CefBrowserHost::CreateBrowser(window_info, handler, url, browser_settings, nullptr, nullptr);

    CefRunMessageLoop();

    CefShutdown();

    return 0;
}
#endif // _WIN32
