// ui_app.hpp - Application CEF
#pragma once

#include "include/cef_app.h"
#include "include/wrapper/cef_message_router.h"

class UIApp : public CefApp, 
			  public CefBrowserProcessHandler,
			  public CefRenderProcessHandler {
public:
	UIApp();

	virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override;
	virtual CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override;

	virtual void OnContextInitialized() override;
	
	// Add command line switches before CEF initialization
	virtual void OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine> command_line) override;

	virtual void OnContextCreated(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) override;

	virtual void OnContextReleased(CefRefPtr<CefBrowser> browser,
 	CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) override;

	virtual bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame, CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override;

private:
	CefRefPtr<CefMessageRouterRendererSide> render_message_router_;
	
	IMPLEMENT_REFCOUNTING(UIApp);
};
