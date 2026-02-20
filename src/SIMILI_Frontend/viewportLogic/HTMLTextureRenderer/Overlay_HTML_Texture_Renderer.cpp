#define GLM_ENABLE_EXPERIMENTAL

#include "Overlay_HTML_Texture_Renderer.hpp"
#include "../../ThirdParty/CEF/cef_binary/include/cef_app.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <windowsx.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif

typedef HGLRC (WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC hDC, HGLRC hShareContext, const int *attribList);

namespace {
	std::string generateRandomAlphanumericID(size_t length) {
		static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
		std::string result;
		result.reserve(length);
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
		for (size_t i = 0; i < length; ++i) {
			result += alphanum[dis(gen)];
		}
		return result;
	}

const char* overlay_html_vertex_shader_source = R"(
#version 460 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 projection;

void main()
{
	gl_Position = projection * vec4(aPos, 0.0, 1.0);
	TexCoord = aTexCoord;
}
)";

const char* overlay_html_fragment_shader_source = R"(
#version 460 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D textureSampler;
uniform vec4 color;
uniform bool useTexture;

void main()
{
	if (useTexture) {
		FragColor = texture(textureSampler, TexCoord);
	} else {
		FragColor = color;
	}
}
)";
}

Overlay_HTML_Texture_Renderer::Overlay_HTML_Texture_Renderer(const std::string& htmlUrl)
	: instance_id_(generateRandomAlphanumericID(16))
	, html_url_(htmlUrl)
	, hwnd_(nullptr)
	, parent_(nullptr)
	, hdc_(nullptr)
	, gl_context_(nullptr)
	, width_(100)
	, height_(100)
	, vao_(0)
	, vbo_(0)
	, shader_program_(0)
	, color_r_(1.0f)
	, color_g_(0.0f)
	, color_b_(0.0f)
	, color_a_(1.0f)
	, texture_id_(0)
	, texture_width_(0)
	, texture_height_(0)
	, browser_(nullptr)
	, rendering_enabled_(false)
	, html_browser_ready_(false)
	, can_receive_inputs_(true)
	, has_parent_(false)
	, parent_name_("")
	, parent_x_(0)
	, parent_y_(0)
	, parent_width_(0)
	, parent_height_(0)
	, parent_dpi_scale_(1.0f)
{
	std::cout << "[Overlay_HTML_Texture_Renderer][" << instance_id_ << "] Instance created for URL: " << html_url_ << std::endl;
}

Overlay_HTML_Texture_Renderer::~Overlay_HTML_Texture_Renderer()
{
	destroy();
}

bool Overlay_HTML_Texture_Renderer::create(HWND parent, int x, int y, int width, int height, HGLRC shareContext)
{
	parent_ = parent;
	width_ = width;
	height_ = height;

	std::wstring windowClassName = L"Overlay_HTML_Texture_Renderer_" + std::wstring(instance_id_.begin(), instance_id_.end());

	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = windowClassName.c_str();

	RegisterClassExW(&wc);

	// Create as POPUP without parent first (like SlotTexture)
	hwnd_ = CreateWindowExW(
		0,  // No extended styles initially
		windowClassName.c_str(),
		L"Overlay HTML Texture",
		WS_POPUP,  // Hidden initially
		x, y, width, height,
		nullptr,  // No parent initially - will be set with SetParent
		nullptr,
		GetModuleHandle(nullptr),
		this
	);

	if (!hwnd_)
	{
		std::cerr << "[Overlay_HTML_Texture_Renderer] Failed to create window" << std::endl;
		return false;
	}

	// Set parent AFTER creation (critical for proper coordinate handling)
	SetParent(hwnd_, parent);
	
	// Add layered and transparent extended styles
	DWORD exStyle = GetWindowLong(hwnd_, GWL_EXSTYLE);
	SetWindowLong(hwnd_, GWL_EXSTYLE, exStyle | WS_EX_LAYERED | WS_EX_TRANSPARENT);
	
	// Set layered attributes
	SetLayeredWindowAttributes(hwnd_, RGB(0, 0, 0), 255, LWA_ALPHA);
	
	// Hide window initially - will be shown when HTML content is ready
	ShowWindow(hwnd_, SW_HIDE);
	
	std::cout << "[Overlay_HTML_Texture_Renderer][" << instance_id_ << "] Window created for URL: " << html_url_ << std::endl;

	hdc_ = GetDC(hwnd_);
	if (!hdc_)
	{
		std::cerr << "[Overlay_HTML_Texture_Renderer] Failed to get DC" << std::endl;
		DestroyWindow(hwnd_);
		hwnd_ = nullptr;
		return false;
	}

	PIXELFORMATDESCRIPTOR pfd = {};
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 32;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE;

	int pixelFormat = ChoosePixelFormat(hdc_, &pfd);
	if (!pixelFormat || !SetPixelFormat(hdc_, pixelFormat, &pfd))
	{
		std::cerr << "[Overlay_HTML_Texture_Renderer] Failed to set pixel format" << std::endl;
		ReleaseDC(hwnd_, hdc_);
		DestroyWindow(hwnd_);
		hwnd_ = nullptr;
		hdc_ = nullptr;
		return false;
	}

	HGLRC previousContext = wglGetCurrentContext();
	HDC previousDC = wglGetCurrentDC();

	initializeOpenGL(shareContext);

	if (!gl_context_)
	{
		std::cerr << "[Overlay_HTML_Texture_Renderer] Failed to initialize OpenGL context" << std::endl;
		ReleaseDC(hwnd_, hdc_);
		DestroyWindow(hwnd_);
		hwnd_ = nullptr;
		hdc_ = nullptr;
		return false;
	}

	wglMakeCurrent(hdc_, gl_context_);

	texture_width_ = width_;
	texture_height_ = height_;
	glGenTextures(1, &texture_id_);
	glBindTexture(GL_TEXTURE_2D, texture_id_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_width_, texture_height_, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	createQuad();

	createBrowser();

	if (previousContext && previousDC)
	{
		wglMakeCurrent(previousDC, previousContext);
	}

	return true;
}

void Overlay_HTML_Texture_Renderer::destroy()
{
	if (being_destroyed_)
	{
		return;
	}

	being_destroyed_ = true;
	rendering_enabled_ = false;
	html_browser_ready_ = false;
	
	if (hwnd_)
	{
		ShowWindow(hwnd_, SW_HIDE);
	}

	std::cout << "[Overlay_HTML_Texture_Renderer][" << instance_id_ << "] Destroying instance for URL: " << html_url_ << std::endl;

	if (browser_)
	{
		if (browser_->GetHost())
		{
			browser_->GetHost()->CloseBrowser(true);
		}

		int wait_count = 0;
		while (!browser_closed_ && wait_count < 100)
		{
			CefDoMessageLoopWork();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			wait_count++;
		}

		browser_ = nullptr;
	}

	if (gl_context_)
	{
		HGLRC previousContext = wglGetCurrentContext();
		HDC previousDC = wglGetCurrentDC();
		
		wglMakeCurrent(hdc_, gl_context_);

		if (texture_id_ != 0)
		{
			glDeleteTextures(1, &texture_id_);
			texture_id_ = 0;
		}

		if (vao_ != 0)
		{
			glDeleteVertexArrays(1, &vao_);
			vao_ = 0;
		}
		if (vbo_ != 0)
		{
			glDeleteBuffers(1, &vbo_);
			vbo_ = 0;
		}
		if (shader_program_ != 0)
		{
			glDeleteProgram(shader_program_);
			shader_program_ = 0;
		}

		wglMakeCurrent(nullptr, nullptr);
		wglDeleteContext(gl_context_);
		gl_context_ = nullptr;
		
		if (previousContext && previousDC && previousContext != gl_context_)
		{
			wglMakeCurrent(previousDC, previousContext);
		}
	}

	if (hdc_)
	{
		ReleaseDC(hwnd_, hdc_);
		hdc_ = nullptr;
	}

	if (hwnd_)
	{
		DestroyWindow(hwnd_);
		hwnd_ = nullptr;
	}
}

void Overlay_HTML_Texture_Renderer::initializeOpenGL(HGLRC shareContext)
{
	HGLRC tempContext = wglCreateContext(hdc_);
	if (!tempContext)
	{
		std::cerr << "[Overlay_HTML_Texture_Renderer] Failed to create temp OpenGL context" << std::endl;
		return;
	}

	wglMakeCurrent(hdc_, tempContext);

	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = 
		(PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

	if (wglCreateContextAttribsARB)
	{
		int attribs[] = {
			WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
			WGL_CONTEXT_MINOR_VERSION_ARB, 6,
			WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
			0
		};

		gl_context_ = wglCreateContextAttribsARB(hdc_, shareContext, attribs);

		if (gl_context_)
		{
			wglMakeCurrent(nullptr, nullptr);
			wglDeleteContext(tempContext);
			wglMakeCurrent(hdc_, gl_context_);
		}
		else
		{
			gl_context_ = tempContext;
		}
	}
	else
	{
		gl_context_ = tempContext;
	}

	if (!gladLoadGLLoader((GLADloadproc)wglGetProcAddress))
	{
		std::cerr << "[Overlay_HTML_Texture_Renderer] Failed to initialize GLAD" << std::endl;
		return;
	}

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Overlay_HTML_Texture_Renderer::createQuad()
{
	float vertices[] = {
		0.0f,          0.0f,          0.0f, 0.0f,
		(float)width_, 0.0f,          1.0f, 0.0f,
		(float)width_, (float)height_, 1.0f, 1.0f,
		0.0f,          0.0f,          0.0f, 0.0f,
		(float)width_, (float)height_, 1.0f, 1.0f,
		0.0f,          (float)height_, 0.0f, 1.0f
	};

	glGenVertexArrays(1, &vao_);
	glGenBuffers(1, &vbo_);

	glBindVertexArray(vao_);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &overlay_html_vertex_shader_source, nullptr);
	glCompileShader(vertexShader);

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &overlay_html_fragment_shader_source, nullptr);
	glCompileShader(fragmentShader);

	shader_program_ = glCreateProgram();
	glAttachShader(shader_program_, vertexShader);
	glAttachShader(shader_program_, fragmentShader);
	glLinkProgram(shader_program_);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	glBindVertexArray(0);
	
	std::cout << "[Overlay_HTML_Texture_Renderer][" << instance_id_ << "] Quad and shaders created" << std::endl;
}

void Overlay_HTML_Texture_Renderer::createBrowser()
{
	CefWindowInfo window_info;
	window_info.SetAsWindowless(0);

	CefBrowserSettings browser_settings;
	browser_settings.windowless_frame_rate = 60;
	browser_settings.javascript = STATE_ENABLED;
	browser_settings.javascript_close_windows = STATE_DISABLED;
	browser_settings.javascript_access_clipboard = STATE_DISABLED;
	browser_settings.image_loading = STATE_ENABLED;
	browser_settings.text_area_resize = STATE_DISABLED;
	browser_settings.tab_to_links = STATE_DISABLED;

	window_info.bounds.x = 0;
	window_info.bounds.y = 0;
	window_info.bounds.width = width_;
	window_info.bounds.height = height_;

	std::cout << "[Overlay_HTML_Texture_Renderer][" << instance_id_ << "] Creating browser for URL: " << html_url_ << std::endl;

	CefBrowserHost::CreateBrowser(window_info, this, html_url_, browser_settings, nullptr, nullptr);
}

void Overlay_HTML_Texture_Renderer::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect)
{
	rect = CefRect(0, 0, width_, height_);
}

void Overlay_HTML_Texture_Renderer::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
const RectList& dirtyRects, const void* buffer, int width, int height)
{
	if (type == PET_VIEW && texture_id_ != 0 && !being_destroyed_)
	{
		HGLRC previousContext = wglGetCurrentContext();
		HDC previousDC = wglGetCurrentDC();
		
		wglMakeCurrent(hdc_, gl_context_);
		updateTexture(buffer, width, height);
		
		if (previousContext && previousDC)
		{
			wglMakeCurrent(previousDC, previousContext);
		}
	}
}

void Overlay_HTML_Texture_Renderer::OnAfterCreated(CefRefPtr<CefBrowser> browser)
{
	browser_ = browser;

	std::cout << "[Overlay_HTML_Texture_Renderer][" << instance_id_ << "] Browser created successfully for URL: " << html_url_ << std::endl;

	if (browser_->GetHost())
	{
		browser_->GetHost()->WasResized();
		browser_->GetHost()->Invalidate(PET_VIEW);
	}

	injectScrollbarEliminationCSS();

	if (on_browser_created_callback_)
	{
		on_browser_created_callback_(browser);
	}

	html_browser_ready_ = true;
	
	if (hwnd_ && rendering_enabled_)
	{
		ShowWindow(hwnd_, SW_SHOW);
		InvalidateRect(hwnd_, nullptr, TRUE);
		UpdateWindow(hwnd_);
		std::cout << "[Overlay_HTML_Texture_Renderer] HTML content ready - showing window" << std::endl;
	}
}

void Overlay_HTML_Texture_Renderer::OnBeforeClose(CefRefPtr<CefBrowser> browser)
{
	being_destroyed_ = true;
	texture_id_ = 0;
	browser_ = nullptr;
	browser_closed_ = true;
}

bool Overlay_HTML_Texture_Renderer::OnConsoleMessage(CefRefPtr<CefBrowser> browser,
cef_log_severity_t level, const CefString& message, const CefString& source, int line)
{
	std::string level_str;
	switch (level)
	{
		case LOGSEVERITY_DEBUG:   level_str = "DEBUG"; break;
		case LOGSEVERITY_INFO:    level_str = "INFO"; break;
		case LOGSEVERITY_WARNING: level_str = "WARN"; break;
		case LOGSEVERITY_ERROR:   level_str = "ERROR"; break;
		default:                  level_str = "LOG"; break;
	}

	std::cout << "[JS Console " << level_str << "] "
			  << message.ToString()
			  << " (" << source.ToString() << ":" << line << ")"
			  << std::endl;

	return false;
}

void Overlay_HTML_Texture_Renderer::OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title)
{
}

void Overlay_HTML_Texture_Renderer::render()
{
	if (being_destroyed_)
	{
		return;
	}
	
	if (!hwnd_ || !rendering_enabled_)
	{
		return;
	}
	
	CefDoMessageLoopWork();
	
	if (!html_browser_ready_)
	{
		return;
	}

	if (gl_context_ && hdc_)
	{
		HGLRC previousContext = wglGetCurrentContext();
		HDC previousDC = wglGetCurrentDC();
		
		wglMakeCurrent(hdc_, gl_context_);

		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, width_, height_);

		renderQuad();

		SwapBuffers(hdc_);
		
		if (previousContext && previousDC)
		{
			wglMakeCurrent(previousDC, previousContext);
		}
	}
}

void Overlay_HTML_Texture_Renderer::renderQuad()
{
	if (texture_id_ == 0)
	{
		return;
	}

	glUseProgram(shader_program_);

	glm::mat4 projection = glm::ortho(0.0f, (float)width_, (float)height_, 0.0f, -1.0f, 1.0f);
	GLint projLoc = glGetUniformLocation(shader_program_, "projection");
	glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

	GLint useTextureLoc = glGetUniformLocation(shader_program_, "useTexture");
	glUniform1i(useTextureLoc, html_browser_ready_ ? 1 : 0);

	if (html_browser_ready_)
	{
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture_id_);
		GLint textureLoc = glGetUniformLocation(shader_program_, "textureSampler");
		glUniform1i(textureLoc, 0);
	}
	else
	{
		GLint colorLoc = glGetUniformLocation(shader_program_, "color");
		glUniform4f(colorLoc, color_r_, color_g_, color_b_, color_a_);
	}

	glBindVertexArray(vao_);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

void Overlay_HTML_Texture_Renderer::SetParentByName(const std::string& parentName)
{
	has_parent_ = true;
	parent_name_ = parentName;
}

void Overlay_HTML_Texture_Renderer::UpdateParentData(int parentX, int parentY, int parentWidth, int parentHeight, float dpiScale)
{
	parent_x_ = parentX;
	parent_y_ = parentY;
	parent_width_ = parentWidth;
	parent_height_ = parentHeight;
	parent_dpi_scale_ = dpiScale;
}

void Overlay_HTML_Texture_Renderer::setTopLeft()
{
	if (!has_parent_)
	{
		return;
	}
	
	// Apply DPI scale to the 100px offset for consistent physical positioning
	int x = parent_x_ + static_cast<int>(10 * parent_dpi_scale_);
	int y = parent_y_;
	setPosition(x, y, width_, height_);
}

void Overlay_HTML_Texture_Renderer::setTopRight()
{
	if (!has_parent_)
	{
		return;
	}
	
	int x = parent_x_ + parent_width_ - width_;
	int y = parent_y_;
	setPosition(x, y, width_, height_);
}

void Overlay_HTML_Texture_Renderer::setBottomLeft()
{
	if (!has_parent_)
	{
		return;
	}
	
	int x = parent_x_;
	int y = parent_y_ + parent_height_ - height_;
	setPosition(x, y, width_, height_);
}

void Overlay_HTML_Texture_Renderer::setBottomRight()
{
	if (!has_parent_)
	{
		return;
	}
	
	int x = parent_x_ + parent_width_ - width_;
	int y = parent_y_ + parent_height_ - height_;
	setPosition(x, y, width_, height_);
}

void Overlay_HTML_Texture_Renderer::setMiddleLeft()
{
	if (!has_parent_)
	{
		return;
	}
	
	int x = parent_x_;
	int y = parent_y_ + (parent_height_ - height_) / 2;
	setPosition(x, y, width_, height_);
}

void Overlay_HTML_Texture_Renderer::setMiddleRight()
{
	if (!has_parent_)
	{
		return;
	}
	
	int x = parent_x_ + parent_width_ - width_;
	int y = parent_y_ + (parent_height_ - height_) / 2;
	setPosition(x, y, width_, height_);
}

void Overlay_HTML_Texture_Renderer::setCenterize()
{
	if (!has_parent_)
	{
		return;
	}
	
	int x = parent_x_ + (parent_width_ - width_) / 2;
	int y = parent_y_ + (parent_height_ - height_) / 2;
	setPosition(x, y, width_, height_);
}

void Overlay_HTML_Texture_Renderer::setPosition(int x, int y, int width, int height)
{
	if (hwnd_)
	{
		SetWindowPos(hwnd_, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);

		if (width != width_ || height != height_)
		{
			width_ = width;
			height_ = height;

			if (gl_context_)
			{
				wglMakeCurrent(hdc_, gl_context_);
				glViewport(0, 0, width_, height_);
			}

			if (texture_id_ != 0)
			{
				glDeleteTextures(1, &texture_id_);
				texture_width_ = width_;
				texture_height_ = height_;
				glGenTextures(1, &texture_id_);
				glBindTexture(GL_TEXTURE_2D, texture_id_);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_width_, texture_height_, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				glBindTexture(GL_TEXTURE_2D, 0);
			}

			if (browser_ && browser_->GetHost())
			{
				browser_->GetHost()->WasResized();
			}
		}
	}
}

void Overlay_HTML_Texture_Renderer::show(bool visible)
{
	if (hwnd_)
	{
		// Only show window if HTML content is ready (avoid showing black background)
		if (visible && !html_browser_ready_)
		{
			// Don't show yet, wait for OnAfterCreated()
			std::cout << "[Overlay_HTML_Texture_Renderer] show(true) called but HTML not ready - deferred" << std::endl;
			return;
		}
		
		ShowWindow(hwnd_, visible ? SW_SHOW : SW_HIDE);

		if (visible)
		{
			InvalidateRect(hwnd_, nullptr, TRUE);
			UpdateWindow(hwnd_);
		}
	}
}

bool Overlay_HTML_Texture_Renderer::isVisible() const
{
	if (hwnd_)
	{
		return IsWindowVisible(hwnd_) != 0;
	}
	return false;
}

void Overlay_HTML_Texture_Renderer::enableRendering(bool enable)
{
	rendering_enabled_ = enable;

	if (enable && hwnd_)
	{
		InvalidateRect(hwnd_, nullptr, TRUE);
	}
}

void Overlay_HTML_Texture_Renderer::updateHTMLTextureSize(int width, int height)
{
	if (width_ != width || height_ != height)
	{
		width_ = width;
		height_ = height;

		if (texture_id_ != 0)
		{
			glDeleteTextures(1, &texture_id_);
			texture_width_ = width_;
			texture_height_ = height_;
			glGenTextures(1, &texture_id_);
			glBindTexture(GL_TEXTURE_2D, texture_id_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_width_, texture_height_, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		if (browser_ && browser_->GetHost())
		{
			browser_->GetHost()->WasResized();
			browser_->GetHost()->Invalidate(PET_VIEW);
		}
	}
}

void Overlay_HTML_Texture_Renderer::invalidate()
{
	if (browser_ && browser_->GetHost())
	{
		browser_->GetHost()->Invalidate(PET_VIEW);
	}
}

void Overlay_HTML_Texture_Renderer::sendKeyEvent(const CefKeyEvent& event)
{
	if (browser_ && browser_->GetHost())
	{
		browser_->GetHost()->SendKeyEvent(event);
	}
}

void Overlay_HTML_Texture_Renderer::injectScrollbarEliminationCSS()
{
	if (!browser_ || !browser_->GetMainFrame())
	{
		return;
	}

	std::string js_code = R"(
		(function() {
			var style = document.createElement('style');
			style.textContent = `
				* {
					overflow: hidden !important;
					-ms-overflow-style: none !important;
					scrollbar-width: none !important;
				}
				*::-webkit-scrollbar {
					display: none !important;
					width: 0 !important;
					height: 0 !important;
				}
				html, body {
					margin: 0 !important;
					padding: 0 !important;
					width: 100% !important;
					height: 100% !important;
					overflow: hidden !important;
				}
			`;
			document.head.appendChild(style);
		})();
	)";

	browser_->GetMainFrame()->ExecuteJavaScript(js_code, browser_->GetMainFrame()->GetURL(), 0);
}

void Overlay_HTML_Texture_Renderer::updateTexture(const void* buffer, int width, int height)
{
	if (!buffer || texture_id_ == 0)
	{
		return;
	}
	
	if (width != texture_width_ || height != texture_height_)
	{
		glDeleteTextures(1, &texture_id_);
		texture_width_ = width;
		texture_height_ = height;
		glGenTextures(1, &texture_id_);
		glBindTexture(GL_TEXTURE_2D, texture_id_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_width_, texture_height_, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	
	glBindTexture(GL_TEXTURE_2D, texture_id_);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture_width_, texture_height_, GL_BGRA, GL_UNSIGNED_BYTE, buffer);
	glBindTexture(GL_TEXTURE_2D, 0);
}

LRESULT CALLBACK Overlay_HTML_Texture_Renderer::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	Overlay_HTML_Texture_Renderer* renderer = nullptr;

	if (msg == WM_CREATE)
	{
		CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
		renderer = static_cast<Overlay_HTML_Texture_Renderer*>(cs->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(renderer));
	}
	else
	{
		renderer = reinterpret_cast<Overlay_HTML_Texture_Renderer*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
	}

	if (renderer)
	{
		switch (msg)
		{
			case WM_PAINT:
			{
				PAINTSTRUCT ps;
				BeginPaint(hwnd, &ps);
				renderer->render();
				EndPaint(hwnd, &ps);
				return 0;
			}

			case WM_ERASEBKGND:
				return 1;

			case WM_CLOSE:
				renderer->show(false);
				return 0;

			case WM_DESTROY:
				return 0;

			case WM_LBUTTONDOWN:
			case WM_LBUTTONUP:
			case WM_RBUTTONDOWN:
			case WM_RBUTTONUP:
			case WM_MOUSEMOVE:
			case WM_MOUSEWHEEL:
			{
				if (renderer->can_receive_inputs_ && renderer->browser_ && renderer->browser_->GetHost())
				{
					CefMouseEvent mouse_event;
					mouse_event.x = GET_X_LPARAM(lParam);
					mouse_event.y = GET_Y_LPARAM(lParam);

					if (msg == WM_MOUSEWHEEL)
					{
						int delta = GET_WHEEL_DELTA_WPARAM(wParam);
						renderer->browser_->GetHost()->SendMouseWheelEvent(mouse_event, 0, delta);
					}
					else
					{
						CefBrowserHost::MouseButtonType button_type = MBT_LEFT;
						bool mouse_up = false;

						if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP)
						{
							button_type = MBT_LEFT;
							mouse_up = (msg == WM_LBUTTONUP);
						}
						else if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP)
						{
							button_type = MBT_RIGHT;
							mouse_up = (msg == WM_RBUTTONUP);
						}

						if (msg == WM_MOUSEMOVE)
						{
							renderer->browser_->GetHost()->SendMouseMoveEvent(mouse_event, false);
						}
						else
						{
							renderer->browser_->GetHost()->SendMouseClickEvent(mouse_event, button_type, mouse_up, 1);
						}
					}
				}
				return 0;
			}
		}
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}