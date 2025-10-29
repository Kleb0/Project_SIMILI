// ui_app.cpp - Implémentation
#include "ui_app.hpp"

UIApp::UIApp() 
{
}

CefRefPtr<CefBrowserProcessHandler> UIApp::GetBrowserProcessHandler() 
{
	return this;
}

CefRefPtr<CefRenderProcessHandler> UIApp::GetRenderProcessHandler() 
{
	return this;
}

void UIApp::OnContextInitialized() 
{
}

void UIApp::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) 
{
	CefMessageRouterConfig config;
	render_message_router_ = CefMessageRouterRendererSide::Create(config);
	render_message_router_->OnContextCreated(browser, frame, context);
}

void UIApp::OnContextReleased(CefRefPtr<CefBrowser> browser,CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) 
{
	if (render_message_router_) 
	{
		render_message_router_->OnContextReleased(browser, frame, context);
	}
}

bool UIApp::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,CefRefPtr<CefFrame> frame,
CefProcessId source_process,CefRefPtr<CefProcessMessage> message) 
{
	if (render_message_router_) 
	{
		return render_message_router_->OnProcessMessageReceived(browser, frame, source_process, message);
	}
	return false;
}

