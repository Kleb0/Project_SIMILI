#include "message_handler.hpp"
#include <iostream>

MessageHandler::MessageHandler(IPCClient* ipc_client) 
	: ipc_client_(ipc_client) 
{
}

bool MessageHandler::OnQuery(CefRefPtr<CefBrowser> browser, 
CefRefPtr<CefFrame> frame, int64_t query_id, const CefString& request, 
bool persistent, CefRefPtr<Callback> callback)
{
	std::string req = request.ToString();
	
	std::cout << "[MessageHandler] Received query: " << req << std::endl;
	
	if (req == "force_repaint")
	{
		// Force CEF to repaint
		std::cout << "[MessageHandler] Forcing browser repaint..." << std::endl;
		browser->GetHost()->WasResized();
		browser->GetHost()->Invalidate(PET_VIEW);
		callback->Success("OK");
		return true;
	}
	
	if (req.substr(0, 4) == "log:") 
	{
		std::string logMessage = req.substr(4);
		std::cout << "[HTML LOG] " << logMessage << std::endl;
		callback->Success("OK");
		return true;
	}
	
	if (req == "request_objects_list") 
	{
		if (ipc_client_) 
		{
			std::cout << "[MessageHandler] Sending request to main process..." << std::endl;
			ipc_client_->send(req);
			callback->Success("OK");
			return true;
		}
		else
		{
			std::cerr << "[MessageHandler] ERROR: No IPC client available!" << std::endl;
			callback->Failure(0, "No IPC client");
			return true;
		}
	}
	
	return false;
}

void MessageHandler::OnQueryCanceled(CefRefPtr<CefBrowser> browser,
CefRefPtr<CefFrame> frame, int64_t query_id)
{
	std::cout << "[MessageHandler] Query canceled: " << query_id << std::endl;
}
