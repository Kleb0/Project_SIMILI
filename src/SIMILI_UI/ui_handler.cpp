#include "ui_handler.hpp"
#include <iostream>
#include <chrono>

UIHandler::UIHandler() : ipc_client_("SIMILI_IPC"), ipc_running_(false)
{
	CefMessageRouterConfig config;
	message_router_ = CefMessageRouterBrowserSide::Create(config);
	
	message_handler_ = new MessageHandler(&ipc_client_);
	message_router_->AddHandler(message_handler_, false);
}

UIHandler::~UIHandler() 
{
	stopIPCListener();
	message_router_->RemoveHandler(message_handler_);
	delete message_handler_;
}

CefRefPtr<CefDisplayHandler> UIHandler::GetDisplayHandler() 
{
	return this;
}

CefRefPtr<CefLifeSpanHandler> UIHandler::GetLifeSpanHandler() 
{
	return this;
}

CefRefPtr<CefLoadHandler> UIHandler::GetLoadHandler() 
{
	return this;
}

CefRefPtr<CefRequestHandler> UIHandler::GetRequestHandler() 
{
	return this;
}

void UIHandler::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) 
{
}

void UIHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) 
{
	CEF_REQUIRE_UI_THREAD();

	browser_list_.push_back(browser);
	
	ipc_client_.send("browser_created");
	
	startIPCListener(browser);
}

bool UIHandler::DoClose(CefRefPtr<CefBrowser> browser) 
{
	CEF_REQUIRE_UI_THREAD();
	return false;
}

void UIHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) 
{
	CEF_REQUIRE_UI_THREAD();

	for (auto it = browser_list_.begin(); it != browser_list_.end(); ++it) 
	{
		if ((*it)->IsSame(browser)) {
			browser_list_.erase(it);
			break;
		}
	}

	if (browser_list_.empty()) 
	{
		ipc_client_.send("browser_closed");
		CefQuitMessageLoop();
	}
}

void UIHandler::OnLoadError(CefRefPtr<CefBrowser> browser,
CefRefPtr<CefFrame> frame, ErrorCode errorCode, const CefString& errorText, const CefString& failedUrl) 
{
	CEF_REQUIRE_UI_THREAD();

	if (errorCode == ERR_ABORTED)
		return;

	std::stringstream ss;

	ss << "<html><body bgcolor=\"white\">"
		"<h2>Failed to load URL "
	   << std::string(failedUrl) << " with error " << std::string(errorText)
	   << " (" << errorCode << ").</h2></body></html>";

	frame->LoadURL(CefString("data:text/html," + ss.str()));
}

bool UIHandler::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
CefRefPtr<CefFrame> frame, CefProcessId source_process, CefRefPtr<CefProcessMessage> message) 
{
	CEF_REQUIRE_UI_THREAD();
	return message_router_->OnProcessMessageReceived(browser, frame, source_process, message);
}

void UIHandler::CloseAllBrowsers(bool force_close) 
{
	if (!CefCurrentlyOn(TID_UI)) {
		return;
	}

	for (auto it = browser_list_.begin(); it != browser_list_.end(); ++it) 
	{
		(*it)->GetHost()->CloseBrowser(force_close);
	}
}

void UIHandler::startIPCListener(CefRefPtr<CefBrowser> browser) 
{
	if (ipc_running_) return;
	
	std::cout << "[UIHandler] Starting IPC listener thread..." << std::endl;
	ipc_running_ = true;
	ipc_thread_ = std::thread([this, browser]() 
	{
		std::cout << "[UIHandler] IPC listener thread started" << std::endl;
		while (ipc_running_) 
		{
			std::string message = ipc_client_.receive();

			if (!message.empty() && browser && browser->GetMainFrame()) 
			{
				std::cout << "[UIHandler] Received IPC message, executing JS..." << std::endl;
				std::string js = "if (window.handleIPCMessage) { window.handleIPCMessage(" + message + "); }";
				browser->GetMainFrame()->ExecuteJavaScript(js, browser->GetMainFrame()->GetURL(), 0);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
		std::cout << "[UIHandler] IPC listener thread stopped" << std::endl;
	});
}

void UIHandler::stopIPCListener() 
{
	if (ipc_running_) 
	{
		ipc_running_ = false;
		if (ipc_thread_.joinable()) 
		{
			ipc_thread_.join();
		}
	}
}
