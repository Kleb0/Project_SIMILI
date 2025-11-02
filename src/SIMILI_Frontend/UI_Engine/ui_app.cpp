// ui_app.cpp - Implémentation
#include "ui_app.hpp"
#include <iostream>

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

void UIApp::OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine> command_line) 
{
	// Disable GPU acceleration to avoid conflicts with OpenGL overlay
	command_line->AppendSwitch("disable-gpu");
	command_line->AppendSwitch("disable-gpu-compositing");
	command_line->AppendSwitch("disable-software-rasterizer");
	command_line->AppendSwitch("disable-gpu-shader-disk-cache");
	command_line->AppendSwitch("disable-gpu-sandbox");
	command_line->AppendSwitch("in-process-gpu");
	command_line->AppendSwitch("disable-features=VizDisplayCompositor");
	
	// Performance optimizations
	command_line->AppendSwitch("disable-smooth-scrolling");
	command_line->AppendSwitch("disable-frame-rate-limit");
	command_line->AppendSwitchWithValue("max-gum-fps", "60");
	
	// Disable animations that can cause lag during resize
	command_line->AppendSwitch("disable-renderer-backgrounding");
	command_line->AppendSwitch("disable-backgrounding-occluded-windows");
}

void UIApp::OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) 
{
	if (!render_message_router_) 
	{
		CefMessageRouterConfig config;
		render_message_router_ = CefMessageRouterRendererSide::Create(config);
		std::cout << "[UIApp] Message router created in render process" << std::endl;
	}
	
	if (render_message_router_) 
	{
		render_message_router_->OnContextCreated(browser, frame, context);
		std::cout << "[UIApp] Context created for frame: " << frame->GetURL().ToString() << std::endl;
	}
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

