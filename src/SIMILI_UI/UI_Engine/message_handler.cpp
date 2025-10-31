#include "message_handler.hpp"
#include "ui_handler.hpp"
#include <iostream>
#include <sstream>

MessageHandler::MessageHandler(IPCClient* ipc_client) 
	: ipc_client_(ipc_client), ui_handler_(nullptr)
{
}

bool MessageHandler::OnQuery(CefRefPtr<CefBrowser> browser, 
CefRefPtr<CefFrame> frame, int64_t query_id, const CefString& request, 
bool persistent, CefRefPtr<Callback> callback)
{
	std::string req = request.ToString();
	
	if (req.find("viewport_resize:") == 0) 
	{
		std::string data = req.substr(16); 
		
		std::istringstream ss(data);
		int x = 0, y = 0, width = 800, height = 600;
		char comma;
		
		if (ss >> x >> comma >> y >> comma >> width >> comma >> height) 
		{
			std::cout << "[MessageHandler] Viewport resize: x=" << x 
			          << " y=" << y << " width=" << width << " height=" << height << std::endl;
			
			if (ui_handler_ && ui_handler_->getOverlay()) 
			{
				ui_handler_->getOverlay()->setPosition(x, y, width, height);
				std::cout << "[MessageHandler] Overlay position updated!" << std::endl;
			}
			else
			{
				std::cout << "[MessageHandler] WARNING: No overlay available!" << std::endl;
			}
		}
		else
		{
			std::cout << "[MessageHandler] ERROR: Failed to parse viewport dimensions: " << data << std::endl;
		}
		
		callback->Success("OK");
		return true;
	}
	
	if (req == "force_repaint")
	{
		browser->GetHost()->WasResized();
		browser->GetHost()->Invalidate(PET_VIEW);
		callback->Success("OK");
		return true;
	}
	
	if (req.substr(0, 4) == "log:") 
	{
		std::string logMessage = req.substr(4);
		std::cout << logMessage << std::endl;
		callback->Success("OK");
		return true;
	}
	
	if (req == "request_objects_list") 
	{
		if (ipc_client_) 
		{
			ipc_client_->send(req);
			callback->Success("OK");
			return true;
		}
		else
		{
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
