#include "HtmlTextureRenderer.hpp"
#include "../../ThirdParty/CEF/cef_binary/include/cef_app.h"
#include <iostream>
#include <thread>
#include <chrono>

HtmlTextureRenderer::HtmlTextureRenderer(TextureRendererTest* textureRenderer)
	: texture_renderer_(textureRenderer)
	, width_(250)
	, height_(100)
	, viewport_hwnd_(nullptr)
{
}

void HtmlTextureRenderer::createBrowser(const std::string& url, int width, int height)
{
	width_ = width;
	height_ = height;
	
	CefWindowInfo window_info;
	window_info.SetAsWindowless(0);
	
	CefBrowserSettings browser_settings;
	browser_settings.windowless_frame_rate = 60;
	browser_settings.javascript = STATE_ENABLED;
	browser_settings.javascript_close_windows = STATE_DISABLED;
	browser_settings.javascript_access_clipboard = STATE_DISABLED;
	
	// Disable scrollbars and optimize for texture rendering
	browser_settings.image_loading = STATE_ENABLED;
	browser_settings.text_area_resize = STATE_DISABLED;
	browser_settings.tab_to_links = STATE_DISABLED;
	
	// Force exact dimensions without scrollbars
	window_info.bounds.x = 0;
	window_info.bounds.y = 0;
	window_info.bounds.width = width;
	window_info.bounds.height = height;
	
	CefBrowserHost::CreateBrowser(window_info, this, url, browser_settings, nullptr, nullptr);
}

void HtmlTextureRenderer::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect)
{
	rect = CefRect(0, 0, width_, height_);
}

void HtmlTextureRenderer::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
const RectList& dirtyRects, const void* buffer, int width, int height)
{
	if (type == PET_VIEW && texture_renderer_) {
		texture_renderer_->updateTexture(buffer, width, height);
	}
}

void HtmlTextureRenderer::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
	browser_ = browser;
	
	if (browser_->GetHost()) 
	{
		browser_->GetHost()->WasHidden(false);
		browser_->GetHost()->SetFocus(true);
		browser_->GetHost()->NotifyScreenInfoChanged();
		browser_->GetHost()->WasResized();
		browser_->GetHost()->Invalidate(PET_VIEW);
	}
	
	// Inject CSS to eliminate scrollbars and ensure 100% fit
	// Use direct execution without complex binding
	injectScrollbarEliminationCSS();
	
	// Notify callback if set
	if (on_browser_created_callback_) {
		on_browser_created_callback_(browser_);
	}
}

void HtmlTextureRenderer::OnBeforeClose(CefRefPtr<CefBrowser> browser)
{
	browser_ = nullptr;
}

bool HtmlTextureRenderer::OnConsoleMessage(CefRefPtr<CefBrowser> browser,
cef_log_severity_t level, const CefString& message, const CefString& source, int line)
{
	// Map CEF log level to readable string
	std::string level_str;
	switch (level) {
		case LOGSEVERITY_DEBUG:   level_str = "DEBUG"; break;
		case LOGSEVERITY_INFO:    level_str = "INFO"; break;
		case LOGSEVERITY_WARNING: level_str = "WARN"; break;
		case LOGSEVERITY_ERROR:   level_str = "ERROR"; break;
		default:                  level_str = "LOG"; break;
	}
	
	// Output to your application's console
	std::cout << "[JS Console " << level_str << "] " 
			  << message.ToString() 
			  << " (" << source.ToString() << ":" << line << ")"
			  << std::endl;
	
	// Return true to suppress the default console message handling
	return true;
}

void HtmlTextureRenderer::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title)
{
	std::string title_str = title.ToString();
	
	if (title_str.find("REPAINT_REQUEST:") == 0) 
	{		
		if (browser_->GetHost()) {
			browser_->GetHost()->Invalidate(PET_VIEW);
		}
		
		if (viewport_hwnd_) 
		{
			InvalidateRect(viewport_hwnd_, nullptr, FALSE);
		}
	}
}

void HtmlTextureRenderer::updateSize(int width, int height)
{
	width_ = width;
	height_ = height;
	
	if (browser_ && browser_->GetHost()) {
		// Update texture renderer size
		if (texture_renderer_) {
			texture_renderer_->initialize(width, height);
		}
		
		// Force CEF to resize and repaint
		browser_->GetHost()->WasResized();
		browser_->GetHost()->NotifyScreenInfoChanged();
		browser_->GetHost()->Invalidate(PET_VIEW);
		
		std::cout << "[HtmlTextureRenderer] Size updated to: " << width << "x" << height << std::endl;
	}
}

void HtmlTextureRenderer::sendKeyEvent(const CefKeyEvent& event)
{
	if (!browser_ || !browser_->GetHost()) 
	{
		return;
	}
		
	browser_->GetHost()->SendKeyEvent(event);
	
	CefRefPtr<CefFrame> frame = browser_->GetMainFrame();
	if (frame) {
		// Convert Windows key code to JavaScript key string
		std::string key_char;
		if (event.windows_key_code >= 48 && event.windows_key_code <= 57) {
			// Number keys 0-9
			key_char = std::string(1, static_cast<char>(event.windows_key_code));
		} else if (event.windows_key_code >= 65 && event.windows_key_code <= 90) {
			// Letter keys A-Z
			key_char = std::string(1, static_cast<char>(event.windows_key_code + 32)); // lowercase
		}
		
		if (!key_char.empty()) 
		{
			std::string js_code = 
				"(function() {"
				"  var evt = new KeyboardEvent('keydown', {"
				"    key: '" + key_char + "',"
				"    code: 'Digit" + key_char + "',"
				"    keyCode: " + std::to_string(event.windows_key_code) + ","
				"    which: " + std::to_string(event.windows_key_code) + ","
				"    bubbles: true,"
				"    cancelable: true"
				"  });"
				"  document.dispatchEvent(evt);"
				"})();";
			
			frame->ExecuteJavaScript(js_code, frame->GetURL(), 0);
		}
	}
}

void HtmlTextureRenderer::injectScrollbarEliminationCSS()
{
	if (!browser_) return;
	
	CefRefPtr<CefFrame> frame = browser_->GetMainFrame();
	if (frame) {
		std::string css_injection = 
			"(function() {"
			"  if (document.readyState === 'loading') {"
			"    document.addEventListener('DOMContentLoaded', function() {"
			"      injectStyles();"
			"    });"
			"  } else {"
			"    injectStyles();"
			"  }"
			"  function injectStyles() {"
			"    var style = document.createElement('style');"
			"    style.textContent = '"
			"      html, body { margin: 0 !important; padding: 0 !important; overflow: hidden !important; width: 100% !important; height: 100% !important; box-sizing: border-box !important; }"
			"      * { box-sizing: border-box !important; }"
			"      ::-webkit-scrollbar { width: 0px !important; height: 0px !important; }"
			"      body::-webkit-scrollbar { display: none !important; }"
			"    ';"
			"    if (document.head) {"
			"      document.head.appendChild(style);"
			"    } else if (document.documentElement) {"
			"      document.documentElement.appendChild(style);"
			"    }"
			"    console.log('SIMILI: CSS scrollbar elimination injected');"
			"  }"
			"})();";
		
		// Execute CSS injection immediately
		frame->ExecuteJavaScript(css_injection, frame->GetURL(), 0);
		std::cout << "[HtmlTextureRenderer] CSS injection executed to eliminate scrollbars" << std::endl;
	}
}
