// ui_handler.hpp - Gestionnaire d'événements CEF
#pragma once

#include "include/cef_client.h"
#include "include/cef_app.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_message_router.h"
#include "ipc_client.hpp"
#include "message_handler.hpp"
#include <list>
#include <sstream>
#include <thread>
#include <atomic>

class UIHandler : public CefClient, public CefDisplayHandler,
				  public CefLifeSpanHandler, public CefLoadHandler, public CefRequestHandler 
{
public:
	explicit UIHandler();
	~UIHandler();

	// CefClient methods
	virtual CefRefPtr<CefDisplayHandler> GetDisplayHandler() override;
	virtual CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
	virtual CefRefPtr<CefLoadHandler> GetLoadHandler() override;
	virtual CefRefPtr<CefRequestHandler> GetRequestHandler() override;

	// CefDisplayHandler methods
	virtual void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override;

	// CefLifeSpanHandler methods
	virtual void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
	virtual bool DoClose(CefRefPtr<CefBrowser> browser) override;
	virtual void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

	// CefLoadHandler methods
	virtual void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, ErrorCode errorCode, 
	const CefString& errorText, const CefString& failedUrl) override;

	// CefRequestHandler methods
	virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
	 CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override;

	void CloseAllBrowsers(bool force_close);
	void startIPCListener(CefRefPtr<CefBrowser> browser);
	void stopIPCListener();

private:
	typedef std::list<CefRefPtr<CefBrowser>> BrowserList;
	BrowserList browser_list_;
	IPCClient ipc_client_;
	CefRefPtr<CefMessageRouterBrowserSide> message_router_;
	MessageHandler* message_handler_;
	std::thread ipc_thread_;
	std::atomic<bool> ipc_running_;

	IMPLEMENT_REFCOUNTING(UIHandler);
};
