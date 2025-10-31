#pragma once

#include "include/cef_client.h"
#include "include/cef_app.h"
#include "include/wrapper/cef_helpers.h"
#include "include/wrapper/cef_message_router.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "ipc_client.hpp"
#include "message_handler.hpp"
#include "overlay_viewport.hpp"
#include <list>
#include <sstream>
#include <thread>
#include <atomic>
#include <memory>

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
	void createOverlayViewport(HWND parent_hwnd);
	void updateOverlayPosition();
	void startRenderTimer();
	void stopRenderTimer();
	void enableOverlayRendering(bool enable);
	bool isOverlayRenderingEnabled() const;
	OverlayViewport* getOverlay() { return overlay_viewport_.get(); }
	HWND getParentHWND() const { return parent_hwnd_; }
	
	// Window procedure hook for resize handling
	static LRESULT CALLBACK ParentWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);

private:
	typedef std::list<CefRefPtr<CefBrowser>> BrowserList;
	BrowserList browser_list_;
	IPCClient ipc_client_;
	CefRefPtr<CefMessageRouterBrowserSide> message_router_;
	MessageHandler* message_handler_;
	std::thread ipc_thread_;
	std::atomic<bool> ipc_running_;
	std::unique_ptr<OverlayViewport> overlay_viewport_;
	HWND parent_hwnd_;
	UINT_PTR timer_id_;

	IMPLEMENT_REFCOUNTING(UIHandler);
};
