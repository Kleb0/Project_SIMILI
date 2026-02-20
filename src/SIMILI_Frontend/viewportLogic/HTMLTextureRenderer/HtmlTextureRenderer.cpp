#include "HtmlTextureRenderer.hpp"
#include "../../ThirdParty/CEF/cef_binary/include/cef_app.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cmath>
#include <windows.h>

HtmlTextureRenderer::HtmlTextureRenderer()
	: texture_id_(0)
	, vao_(0)
	, vbo_(0)
	, shader_program_(0)
	, width_(250)
	, height_(100)
	, texture_width_(250)
	, texture_height_(100)
	, initialized_(false)
	, render_x_(10)
	, render_y_(10)
	, render_width_(250)
	, render_height_(100)
	, viewport_hwnd_(nullptr)
{
}

HtmlTextureRenderer::~HtmlTextureRenderer()
{
	cleanup();
}

void HtmlTextureRenderer::initialize(int width, int height)
{
	if (initialized_) 
	{
		return;
	}

	width_ = width;
	height_ = height;

	createTexture(texture_width_, texture_height_);
	createQuadMesh();
	createShaderProgram();

	initialized_ = true;
}

void HtmlTextureRenderer::cleanup()
{
	if (!initialized_) {
		return;
	}
	
	HGLRC currentContext = wglGetCurrentContext();
	if (!currentContext) {
		std::cerr << "[HtmlTextureRenderer] Warning: cleanup() called without active OpenGL context" << std::endl;
		initialized_ = false;
		return;
	}

	if (texture_id_ != 0) {
		glDeleteTextures(1, &texture_id_);
		texture_id_ = 0;
	}

	if (vao_ != 0) {
		glDeleteVertexArrays(1, &vao_);
		vao_ = 0;
	}

	if (vbo_ != 0) {
		glDeleteBuffers(1, &vbo_);
		vbo_ = 0;
	}

	if (shader_program_ != 0) {
		glDeleteProgram(shader_program_);
		shader_program_ = 0;
	}

	initialized_ = false;
}

void HtmlTextureRenderer::createTexture(int tex_width, int tex_height)
{
	texture_width_ = tex_width;
	texture_height_ = tex_height;
	
	glGenTextures(1, &texture_id_);
	glBindTexture(GL_TEXTURE_2D, texture_id_);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_width_, texture_height_, 0, 
				 GL_BGRA, GL_UNSIGNED_BYTE, nullptr);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);
}

void HtmlTextureRenderer::createQuadMesh()
{
	float vertices[] = 
	{
		0.0f, 1.0f, 0.0f,   0.0f, 1.0f, 
		0.0f, 0.0f, 0.0f,   0.0f, 0.0f,  
		1.0f, 0.0f, 0.0f,   1.0f, 0.0f,
		 
		0.0f, 1.0f, 0.0f,   0.0f, 1.0f,
		1.0f, 0.0f, 0.0f,   1.0f, 0.0f, 
		1.0f, 1.0f, 0.0f,   1.0f, 1.0f  
	};

	glGenVertexArrays(1, &vao_);
	glGenBuffers(1, &vbo_);

	glBindVertexArray(vao_);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);
}

GLuint HtmlTextureRenderer::compileShader(GLenum type, const char* source)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, nullptr);
	glCompileShader(shader);

	GLint success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) 
	{
		char infoLog[512];
		glGetShaderInfoLog(shader, 512, nullptr, infoLog);
		std::cerr << "[HtmlTextureRenderer] Shader compilation failed:\n" << infoLog << std::endl;
	}

	return shader;
}

void HtmlTextureRenderer::createShaderProgram()
{
	const char* vertexShaderSource = R"(
		#version 460 core
		layout (location = 0) in vec3 aPos;
		layout (location = 1) in vec2 aTexCoord;
		
		out vec2 TexCoord;
		
		uniform vec2 viewportSize;
		uniform vec2 rectSize;
		uniform vec2 rectPos;
		
		void main()
		{
			vec2 pixelPos = aPos.xy * rectSize + rectPos;
			vec2 ndc = (pixelPos / viewportSize) * 2.0 - 1.0;
			ndc.y = -ndc.y;
			
			gl_Position = vec4(ndc, 0.0, 1.0);
			TexCoord = aTexCoord;
		}
	)";

	const char* fragmentShaderSource = R"(
		#version 460 core
		out vec4 FragColor;
		
		in vec2 TexCoord;
		uniform sampler2D texture1;
		
		void main()
		{
			FragColor = texture(texture1, TexCoord);
		}
	)";

	GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
	GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);

	shader_program_ = glCreateProgram();
	glAttachShader(shader_program_, vertexShader);
	glAttachShader(shader_program_, fragmentShader);
	glLinkProgram(shader_program_);

	GLint success;
	glGetProgramiv(shader_program_, GL_LINK_STATUS, &success);
	if (!success) {
		char infoLog[512];
		glGetProgramInfoLog(shader_program_, 512, nullptr, infoLog);
		std::cerr << "[HtmlTextureRenderer] Shader linking failed:\n" << infoLog << std::endl;
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
}

void HtmlTextureRenderer::render()
{
	if (!initialized_) {
		return;
	}

	GLboolean depthTestEnabled;
	glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
	GLboolean blendEnabled;
	glGetBooleanv(GL_BLEND, &blendEnabled);

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(shader_program_);

	glUniform2f(glGetUniformLocation(shader_program_, "viewportSize"), 
				static_cast<float>(width_), static_cast<float>(height_));
	glUniform2f(glGetUniformLocation(shader_program_, "rectSize"), 
				static_cast<float>(render_width_), static_cast<float>(render_height_));
	glUniform2f(glGetUniformLocation(shader_program_, "rectPos"), 
				static_cast<float>(render_x_), static_cast<float>(render_y_));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_id_);
	glUniform1i(glGetUniformLocation(shader_program_, "texture1"), 0);

	glBindVertexArray(vao_);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);

	if (depthTestEnabled) glEnable(GL_DEPTH_TEST);
	if (!blendEnabled) glDisable(GL_BLEND);
}

void HtmlTextureRenderer::updateTexture(const void* buffer, int width, int height)
{
	if (!initialized_ || !buffer) {
		return;
	}
	
	if (width != texture_width_ || height != texture_height_) {
		if (texture_id_ != 0) {
			glDeleteTextures(1, &texture_id_);
		}
		createTexture(width, height);
	}
	
	glBindTexture(GL_TEXTURE_2D, texture_id_);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture_width_, texture_height_,
					GL_BGRA, GL_UNSIGNED_BYTE, buffer);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void HtmlTextureRenderer::resize(int width, int height)
{
	if (width == width_ && height == height_) {
		return;
	}

	width_ = width;
	height_ = height;
}

void HtmlTextureRenderer::setRenderRect(int x, int y, int w, int h)
{
	render_x_ = x;
	render_y_ = y;
	render_width_ = w;
	render_height_ = h;
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
	if (type == PET_VIEW && !being_destroyed_) {
		updateTexture(buffer, width, height);
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
	std::cout << "[HtmlTextureRenderer] OnBeforeClose called - browser is fully closed" << std::endl;
	being_destroyed_ = true;
	browser_ = nullptr;
	browser_closed_ = true;
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
