// ui_handler.hpp - Gestionnaire d'événements CEF
#pragma once

#include "include/cef_client.h"
#include "include/cef_app.h"
#include "include/wrapper/cef_helpers.h"
#include "ipc_client.hpp"
#include <list>
#include <sstream>

class UIHandler : public CefClient,
                  public CefDisplayHandler,
                  public CefLifeSpanHandler,
                  public CefLoadHandler {
public:
    explicit UIHandler() : ipc_client_("SIMILI_IPC") {}

    // CefClient methods
    virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

    // CefDisplayHandler methods
    virtual void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override {
        // Titre de la fenêtre changé
    }

    // CefLifeSpanHandler methods
    virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        CEF_REQUIRE_UI_THREAD();
        browser_list_.push_back(browser);
        
        // Notifier le processus principal
        ipc_client_.send("browser_created");
    }

    virtual bool DoClose(CefRefPtr<CefBrowser> browser) override {
        CEF_REQUIRE_UI_THREAD();
        return false;
    }

    virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
        CEF_REQUIRE_UI_THREAD();

        // Retirer de la liste
        for (auto it = browser_list_.begin(); it != browser_list_.end(); ++it) {
            if ((*it)->IsSame(browser)) {
                browser_list_.erase(it);
                break;
            }
        }

        // Quitter si plus de browsers
        if (browser_list_.empty()) {
            ipc_client_.send("browser_closed");
            // Quitter la boucle de messages CEF
            CefQuitMessageLoop();
        }
    }

    // CefLoadHandler methods
    virtual void OnLoadError(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            ErrorCode errorCode,
                            const CefString& errorText,
                            const CefString& failedUrl) override {
        CEF_REQUIRE_UI_THREAD();

        if (errorCode == ERR_ABORTED)
            return;

        // Afficher une page d'erreur
        std::stringstream ss;
        ss << "<html><body bgcolor=\"white\">"
              "<h2>Failed to load URL "
           << std::string(failedUrl) << " with error " << std::string(errorText)
           << " (" << errorCode << ").</h2></body></html>";
        frame->LoadURL(CefString("data:text/html," + ss.str()));
    }

    void CloseAllBrowsers(bool force_close) {
        if (!CefCurrentlyOn(TID_UI)) {
            return;
        }

        for (auto it = browser_list_.begin(); it != browser_list_.end(); ++it) {
            (*it)->GetHost()->CloseBrowser(force_close);
        }
    }

private:
    typedef std::list<CefRefPtr<CefBrowser>> BrowserList;
    BrowserList browser_list_;
    IPCClient ipc_client_;

    IMPLEMENT_REFCOUNTING(UIHandler);
};
