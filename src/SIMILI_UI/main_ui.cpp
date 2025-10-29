#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_sandbox_win.h"
#include "ui_app.hpp"
#include "ui_handler.hpp"
#include "borderless_window.hpp"

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

    BorderlessWindow window("SIMILI - Object List", 800, 600, 100, 100);
    HWND hwnd = window.create();

    if (!hwnd) {
        CefShutdown();
        return -1;
    }

    CefWindowInfo window_info;
    RECT rect;
    GetClientRect(hwnd, &rect);
    
    CefRect cef_rect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    window_info.SetAsChild(hwnd, cef_rect);

    CefBrowserSettings browser_settings;
    browser_settings.windowless_frame_rate = 60; 

    CefRefPtr<UIHandler> handler(new UIHandler);

    std::string url = "file:///e:/Project_SIMILI/Project/ui/object_list.html";

    CefBrowserHost::CreateBrowser(window_info, handler, url, browser_settings, nullptr, nullptr);

    CefRunMessageLoop();

    CefShutdown();

    return 0;
}
#endif 
