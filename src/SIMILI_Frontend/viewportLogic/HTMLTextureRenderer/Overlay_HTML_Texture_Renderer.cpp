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
	std::string generateRandomAlphanumericID(size_t length) 
	{
		static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
		std::string result;
		result.reserve(length);
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);
		for (size_t i = 0; i < length; ++i) 
		{
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
	, base_width_(100)
	, base_height_(100)
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
	, use_direct_composition_(false)
	, d2d_factory_(nullptr)
	, d3d11_device_(nullptr)
	, d3d11_device_context_(nullptr)
	, dxgi_device_(nullptr)
	, d2d_device_(nullptr)
	, d2d_device_context_(nullptr)
	, dxgi_swap_chain_(nullptr)
	, d2d_target_bitmap_(nullptr)
	, dcomp_device_(nullptr)
	, dcomp_target_(nullptr)
	, cef_bitmap_(nullptr)
	, transparency_enabled_(false)
	, transparency_alpha_(1.0f)
	, using_shared_devices_(false)
	, filter_color_enabled_(false)
	, filter_r_(0)
	, filter_g_(0)
	, filter_b_(0)
	, scale_factor_(1.0f)
	, maximised_(false)
{
	std::cout << "[Overlay_HTML_Texture_Renderer][" << instance_id_ << "] Instance created for URL: " << html_url_ << std::endl;
}

Overlay_HTML_Texture_Renderer::~Overlay_HTML_Texture_Renderer()
{
	destroy();
}

void Overlay_HTML_Texture_Renderer::create(HWND parent, int x, int y, int width, int height, HGLRC shareContext)
{
	parent_ = parent;
	base_width_ = width;
	base_height_ = height;
	width_ = static_cast<int>(width * scale_factor_);
	height_ = static_cast<int>(height * scale_factor_);

	std::wstring windowClassName = L"Overlay_HTML_Texture_Renderer_" + std::wstring(instance_id_.begin(), instance_id_.end());

	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = windowClassName.c_str();

	RegisterClassExW(&wc);

	hwnd_ = CreateWindowExW(
		0,
		windowClassName.c_str(),
		L"Overlay HTML Texture",
		WS_POPUP,
		x, y, width, height,
		use_direct_composition_ ? parent : nullptr,
		nullptr,
		GetModuleHandle(nullptr),
		this
	);

	if (!hwnd_)
	{
		std::cerr << "[Overlay_HTML_Texture_Renderer] Failed to create window" << std::endl;
		return;
	}

	if (!use_direct_composition_)
	{
		SetParent(hwnd_, parent);
	}
	
	DWORD exStyle = GetWindowLong(hwnd_, GWL_EXSTYLE);
	
	if (use_direct_composition_)
	{
		SetWindowLong(hwnd_, GWL_EXSTYLE, exStyle | WS_EX_NOREDIRECTIONBITMAP);
	}
	else
	{
		SetWindowLong(hwnd_, GWL_EXSTYLE, exStyle | WS_EX_LAYERED | WS_EX_TRANSPARENT);
		SetLayeredWindowAttributes(hwnd_, RGB(0, 0, 0), 255, LWA_ALPHA);
	}
	
	ShowWindow(hwnd_, SW_HIDE);
	
	std::cout << "[Overlay_HTML_Texture_Renderer][" << instance_id_ << "] Window created for URL: " << html_url_ << std::endl;

	hdc_ = GetDC(hwnd_);
	if (!hdc_)
	{
		std::cerr << "[Overlay_HTML_Texture_Renderer] Failed to get DC" << std::endl;
		DestroyWindow(hwnd_);
		hwnd_ = nullptr;
		return;
	}

	if (use_direct_composition_)
	{
		initializeDirectComposition();
		createDirectCompositionResources();
	}
	else
	{
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
			return;
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
			return;
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

		if (previousContext && previousDC)
		{
			wglMakeCurrent(previousDC, previousContext);
		}
	}

	createBrowser();
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

	if (use_direct_composition_)
	{
		cleanupDirectComposition();
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
	if (maximised_ && scale_factor_ > 0.0f)
	{
		int render_width = static_cast<int>(width_ / scale_factor_);
		int render_height = static_cast<int>(height_ / scale_factor_);
		rect = CefRect(0, 0, render_width, render_height);
	}
	else
	{
		rect = CefRect(0, 0, width_, height_);
	}
}

void Overlay_HTML_Texture_Renderer::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
const RectList& dirtyRects, const void* buffer, int width, int height)
{
	if (type == PET_VIEW && !being_destroyed_)
	{
		if (use_direct_composition_)
		{
			updateD2DBitmap(buffer, width, height);
		}
		else
		{
			if (texture_id_ != 0)
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

	if (use_direct_composition_)
	{
		renderDirectComposition();
	}
	else
	{
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

void Overlay_HTML_Texture_Renderer::maximise()
{
	maximised_ = true;
	if (browser_)
	{
		browser_->GetHost()->WasResized();
	}
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
		int finalX = x;
		int finalY = y;
		
		if (use_direct_composition_ && parent_)
		{
			RECT parentRect;
			if (GetWindowRect(parent_, &parentRect))
			{
				int parentWidth = parentRect.right - parentRect.left;
				int parentHeight = parentRect.bottom - parentRect.top;
				
				if (finalX < parentRect.left)
					finalX = parentRect.left;
				if (finalY < parentRect.top)
					finalY = parentRect.top;
				if (finalX + width > parentRect.right)
					finalX = parentRect.right - width;
				if (finalY + height > parentRect.bottom)
					finalY = parentRect.bottom - height;
			}
		}
		
		SetWindowPos(hwnd_, nullptr, finalX, finalY, width, height, SWP_NOZORDER | SWP_NOACTIVATE);

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
			
			if (use_direct_composition_ && dxgi_swap_chain_ && d2d_device_context_)
			{
				d2d_device_context_->SetTarget(nullptr);
				if (d2d_target_bitmap_)
				{
					d2d_target_bitmap_->Release();
					d2d_target_bitmap_ = nullptr;
				}
				
				HRESULT hr = dxgi_swap_chain_->ResizeBuffers(2, width_, height_, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
				
				if (SUCCEEDED(hr))
				{
					D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
						D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
						D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
						0,
						0,
						nullptr
					);
					unsigned int nDPI = GetDpiForWindow(hwnd_);
					bitmapProperties.dpiX = nDPI;
					bitmapProperties.dpiY = nDPI;

					IDXGISurface* pDXGISurface = nullptr;
					hr = dxgi_swap_chain_->GetBuffer(0, __uuidof(IDXGISurface), (void**)&pDXGISurface);
					if (SUCCEEDED(hr))
					{
						hr = d2d_device_context_->CreateBitmapFromDxgiSurface(pDXGISurface, bitmapProperties, &d2d_target_bitmap_);
						if (SUCCEEDED(hr))
						{
							d2d_device_context_->SetTarget(d2d_target_bitmap_);
						}
						pDXGISurface->Release();
					}
				}
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

			case WM_WINDOWPOSCHANGING:
			{
				if (renderer->use_direct_composition_ && renderer->parent_)
				{
					WINDOWPOS* pwp = reinterpret_cast<WINDOWPOS*>(lParam);
					RECT parentRect;
					if (GetWindowRect(renderer->parent_, &parentRect))
					{
						if (pwp->x < parentRect.left)
							pwp->x = parentRect.left;
						if (pwp->y < parentRect.top)
							pwp->y = parentRect.top;
						if (pwp->x + pwp->cx > parentRect.right)
							pwp->x = parentRect.right - pwp->cx;
						if (pwp->y + pwp->cy > parentRect.bottom)
							pwp->y = parentRect.bottom - pwp->cy;
					}
				}
				break;
			}
			
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
void Overlay_HTML_Texture_Renderer::initializeDirectComposition()
{
	HRESULT hr = S_OK;
	
	if (!using_shared_devices_)
	{
		D2D1_FACTORY_OPTIONS options = {};
		options.debugLevel = D2D1_DEBUG_LEVEL_NONE;
		hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &options, (void**)&d2d_factory_);
	}
	else
	{
		hr = S_OK;
	}
	
	if (SUCCEEDED(hr) && !using_shared_devices_)
	{
		UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
		D3D_FEATURE_LEVEL featureLevels[] = {
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
			D3D_FEATURE_LEVEL_9_3,
			D3D_FEATURE_LEVEL_9_2,
			D3D_FEATURE_LEVEL_9_1
		};
		D3D_FEATURE_LEVEL featureLevel;
		hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			0,
			creationFlags,
			featureLevels,
			ARRAYSIZE(featureLevels),
			D3D11_SDK_VERSION,
			&d3d11_device_,
			&featureLevel,
			&d3d11_device_context_
		);

		if (SUCCEEDED(hr))
		{
			hr = d3d11_device_->QueryInterface(__uuidof(IDXGIDevice1), (void**)&dxgi_device_);
		}
	}

	if (SUCCEEDED(hr) && d2d_factory_ && !using_shared_devices_)
	{
		hr = d2d_factory_->CreateDevice(dxgi_device_, &d2d_device_);
	}

	if (SUCCEEDED(hr) && d2d_device_)
	{
		ID2D1DeviceContext* pD2DDeviceContext = nullptr;
		hr = d2d_device_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &pD2DDeviceContext);
		if (SUCCEEDED(hr))
		{
			hr = pD2DDeviceContext->QueryInterface(__uuidof(ID2D1DeviceContext3), (void**)&d2d_device_context_);
			pD2DDeviceContext->Release();
		}
	}
}
void Overlay_HTML_Texture_Renderer::createDirectCompositionResources()
{
	HRESULT hr = S_OK;
	
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.Width = width_;
	swapChainDesc.Height = height_;
	swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	swapChainDesc.Flags = 0;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
	
	IDXGIAdapter* pDXGIAdapter = nullptr;
	hr = dxgi_device_->GetAdapter(&pDXGIAdapter);
	if (SUCCEEDED(hr))
	{
		IDXGIFactory2* pDXGIFactory2 = nullptr;
		hr = pDXGIAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&pDXGIFactory2);
		if (SUCCEEDED(hr))
		{
			hr = pDXGIFactory2->CreateSwapChainForComposition(d3d11_device_, &swapChainDesc, nullptr, &dxgi_swap_chain_);
			if (SUCCEEDED(hr))
			{
				hr = dxgi_device_->SetMaximumFrameLatency(1);
			}
			pDXGIFactory2->Release();
		}
		pDXGIAdapter->Release();
	}
	
	if (SUCCEEDED(hr))
	{
		D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
			0,
			0,
			nullptr
		);
		unsigned int nDPI = GetDpiForWindow(hwnd_);
		bitmapProperties.dpiX = nDPI;
		bitmapProperties.dpiY = nDPI;

		IDXGISurface* pDXGISurface = nullptr;
		if (dxgi_swap_chain_)
		{
			hr = dxgi_swap_chain_->GetBuffer(0, __uuidof(IDXGISurface), (void**)&pDXGISurface);
			if (SUCCEEDED(hr))
			{
				hr = d2d_device_context_->CreateBitmapFromDxgiSurface(pDXGISurface, bitmapProperties, &d2d_target_bitmap_);
				if (SUCCEEDED(hr))
				{
					d2d_device_context_->SetTarget(d2d_target_bitmap_);
				}
				pDXGISurface->Release();
			}
		}
	}
	
	if (SUCCEEDED(hr))
	{
		hr = DCompositionCreateDevice(dxgi_device_, __uuidof(IDCompositionDevice), (void**)&dcomp_device_);
		if (SUCCEEDED(hr))
		{
			hr = dcomp_device_->CreateTargetForHwnd(hwnd_, true, &dcomp_target_);
			if (SUCCEEDED(hr))
			{
				IDCompositionVisual* pDCompositionVisual = nullptr;
				hr = dcomp_device_->CreateVisual(&pDCompositionVisual);
				if (SUCCEEDED(hr))
				{
					hr = pDCompositionVisual->SetContent(dxgi_swap_chain_);
					hr = dcomp_target_->SetRoot(pDCompositionVisual);
					hr = dcomp_device_->Commit();
					pDCompositionVisual->Release();
				}
			}
		}
	}
}

void Overlay_HTML_Texture_Renderer::renderDirectComposition()
{
	if (!d2d_device_context_ || !dxgi_swap_chain_)
	{
		return;
	}
	
	HRESULT hr = S_OK;
	d2d_device_context_->BeginDraw();
	
	D2D1_SIZE_F size = d2d_device_context_->GetSize();
	
	d2d_device_context_->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
	
	if (cef_bitmap_)
	{
		D2D1_RECT_F destRect = D2D1::RectF(0.0f, 0.0f, size.width, size.height);
		d2d_device_context_->DrawBitmap(cef_bitmap_, destRect, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
	}
	
	hr = d2d_device_context_->EndDraw();
	
	if (SUCCEEDED(hr))
	{
		hr = dxgi_swap_chain_->Present(1, 0);
	}
}

void Overlay_HTML_Texture_Renderer::cleanupDirectComposition()
{
	if (cef_bitmap_)
	{
		cef_bitmap_->Release();
		cef_bitmap_ = nullptr;
	}
	if (dcomp_target_)
	{
		dcomp_target_->Release();
		dcomp_target_ = nullptr;
	}
	if (dcomp_device_)
	{
		dcomp_device_->Release();
		dcomp_device_ = nullptr;
	}
	if (d2d_target_bitmap_)
	{
		d2d_target_bitmap_->Release();
		d2d_target_bitmap_ = nullptr;
	}
	if (dxgi_swap_chain_)
	{
		dxgi_swap_chain_->Release();
		dxgi_swap_chain_ = nullptr;
	}
	if (d2d_device_context_)
	{
		d2d_device_context_->Release();
		d2d_device_context_ = nullptr;
	}
	
	if (!using_shared_devices_)
	{
		if (d2d_device_)
		{
			d2d_device_->Release();
			d2d_device_ = nullptr;
		}
		if (dxgi_device_)
		{
			dxgi_device_->Release();
			dxgi_device_ = nullptr;
		}
		if (d3d11_device_context_)
		{
			d3d11_device_context_->Release();
			d3d11_device_context_ = nullptr;
		}
		if (d3d11_device_)
		{
			d3d11_device_->Release();
			d3d11_device_ = nullptr;
		}
		if (d2d_factory_)
		{
			d2d_factory_->Release();
			d2d_factory_ = nullptr;
		}
	}
	else
	{
		d2d_device_ = nullptr;
		dxgi_device_ = nullptr;
		d3d11_device_context_ = nullptr;
		d3d11_device_ = nullptr;
		d2d_factory_ = nullptr;
	}
}

void Overlay_HTML_Texture_Renderer::EnableTransparency(float alpha)
{
	transparency_enabled_ = true;
	transparency_alpha_ = alpha;
	
	if (use_direct_composition_ && hwnd_)
	{
		DWORD exStyle = GetWindowLong(hwnd_, GWL_EXSTYLE);
		exStyle &= ~WS_EX_LAYERED;
		exStyle &= ~WS_EX_TRANSPARENT;
		exStyle |= WS_EX_NOREDIRECTIONBITMAP;
		SetWindowLong(hwnd_, GWL_EXSTYLE, exStyle);
	}
}

void Overlay_HTML_Texture_Renderer::FilterColor(int r, int g, int b)
{
	filter_color_enabled_ = true;
	filter_r_ = r;
	filter_g_ = g;
	filter_b_ = b;
	
	if (use_direct_composition_ && hwnd_)
	{
		DWORD exStyle = GetWindowLong(hwnd_, GWL_EXSTYLE);
		exStyle &= ~WS_EX_LAYERED;
		exStyle &= ~WS_EX_TRANSPARENT;
		exStyle |= WS_EX_NOREDIRECTIONBITMAP;
		SetWindowLong(hwnd_, GWL_EXSTYLE, exStyle);
	}
}

void Overlay_HTML_Texture_Renderer::DisableColorFilter()
{
	filter_color_enabled_ = false;
}

void Overlay_HTML_Texture_Renderer::changeScaleByValue(float scale)
{
	scale_factor_ *= scale;
	if (scale_factor_ < 0.1f)
	{
		scale_factor_ = 0.1f;
	}
	if (scale_factor_ > 10.0f)
	{
		scale_factor_ = 10.0f;
	}
	
	int new_width = static_cast<int>(base_width_ * scale_factor_);
	int new_height = static_cast<int>(base_height_ * scale_factor_);
	
	if (new_width != width_ || new_height != height_)
	{
		if (hwnd_)
		{
			RECT rect;
			GetWindowRect(hwnd_, &rect);
			int current_x = rect.left;
			int current_y = rect.top;
			
			int offset_x = (width_ - new_width) / 2;
			int offset_y = (height_ - new_height) / 2;
			
			setPosition(current_x + offset_x, current_y + offset_y, new_width, new_height);
		}
		else
		{
			updateHTMLTextureSize(new_width, new_height);
		}
	}
}

void Overlay_HTML_Texture_Renderer::setSharedDevices(ID3D11Device* d3d11Device, IDXGIDevice1* dxgiDevice, ID2D1Factory1* d2dFactory, ID2D1Device* d2dDevice)
{
	using_shared_devices_ = true;
	d3d11_device_ = d3d11Device;
	dxgi_device_ = dxgiDevice;
	d2d_factory_ = d2dFactory;
	d2d_device_ = d2dDevice;
}

void Overlay_HTML_Texture_Renderer::updateD2DBitmap(const void* buffer, int width, int height)
{
	if (!d2d_device_context_)
	{
		return;
	}
	
	HRESULT hr = S_OK;
	
	if (!cef_bitmap_ || texture_width_ != width || texture_height_ != height)
	{
		if (cef_bitmap_)
		{
			cef_bitmap_->Release();
			cef_bitmap_ = nullptr;
		}
		
		texture_width_ = width;
		texture_height_ = height;
		
		D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_NONE,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
			0,
			0,
			nullptr
		);
		
		hr = d2d_device_context_->CreateBitmap(
			D2D1::SizeU(width, height),
			nullptr,
			0,
			bitmapProperties,
			&cef_bitmap_
		);
	}
	
	if (SUCCEEDED(hr) && cef_bitmap_)
	{
		D2D1_RECT_U destRect = D2D1::RectU(0, 0, width, height);
		
		if (transparency_enabled_ || filter_color_enabled_)
		{
			int pixelCount = width * height;
			unsigned char* modifiedBuffer = new unsigned char[pixelCount * 4];
			memcpy(modifiedBuffer, buffer, pixelCount * 4);
			
			for (int i = 0; i < pixelCount; ++i)
			{
				int idx = i * 4;
				unsigned char b = modifiedBuffer[idx];
				unsigned char g = modifiedBuffer[idx + 1];
				unsigned char r = modifiedBuffer[idx + 2];
				unsigned char a = modifiedBuffer[idx + 3];
				
				bool shouldFilter = false;
				
				if (filter_color_enabled_)
				{
					int tolerance = 10;
					if (abs(r - filter_r_) <= tolerance && 
						abs(g - filter_g_) <= tolerance && 
						abs(b - filter_b_) <= tolerance)
					{
						shouldFilter = true;
					}
				}
				
				if (shouldFilter)
				{
					modifiedBuffer[idx] = 0;
					modifiedBuffer[idx + 1] = 0;
					modifiedBuffer[idx + 2] = 0;
					modifiedBuffer[idx + 3] = 0;
				}
				else if (transparency_enabled_)
				{
					float finalAlpha = (a / 255.0f) * transparency_alpha_;
					modifiedBuffer[idx] = static_cast<unsigned char>(b * finalAlpha);
					modifiedBuffer[idx + 1] = static_cast<unsigned char>(g * finalAlpha);
					modifiedBuffer[idx + 2] = static_cast<unsigned char>(r * finalAlpha);
					modifiedBuffer[idx + 3] = static_cast<unsigned char>(a * transparency_alpha_);
				}
			}
			
			hr = cef_bitmap_->CopyFromMemory(&destRect, modifiedBuffer, width * 4);
			delete[] modifiedBuffer;
		}
		else
		{
			hr = cef_bitmap_->CopyFromMemory(&destRect, buffer, width * 4);
		}
	}
}