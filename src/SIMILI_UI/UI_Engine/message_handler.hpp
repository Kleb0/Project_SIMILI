#pragma once

#include "include/wrapper/cef_message_router.h"
#include "ipc_client.hpp"
#include <string>

// Forward declaration to avoid circular dependency
class UIHandler;

class MessageHandler : public CefMessageRouterBrowserSide::Handler 
{
public:
	explicit MessageHandler(IPCClient* ipc_client);

	virtual bool OnQuery(CefRefPtr<CefBrowser> browser, 
	CefRefPtr<CefFrame> frame, int64_t query_id,const CefString& request, 
	bool persistent, CefRefPtr<Callback> callback) override;

	virtual void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
	CefRefPtr<CefFrame> frame, int64_t query_id) override;
	
	void setUIHandler(UIHandler* handler) { ui_handler_ = handler; }

private:
	IPCClient* ipc_client_;
	UIHandler* ui_handler_;
};
