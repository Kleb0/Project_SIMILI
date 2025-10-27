// ui_app.hpp - Application CEF
#pragma once

#include "include/cef_app.h"

class UIApp : public CefApp, public CefBrowserProcessHandler {
public:
    UIApp() = default;

    // CefApp methods
    virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
        return this;
    }

    // CefBrowserProcessHandler methods
    virtual void OnContextInitialized() override {
        // Appelé quand CEF est initialisé
    }

private:
    IMPLEMENT_REFCOUNTING(UIApp);
};
