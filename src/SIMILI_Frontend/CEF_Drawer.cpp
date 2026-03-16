#include "CEF_Drawer.hpp"
#include <string>
#include "include/cef_browser.h"

// Vertex shader - simple pass-through with texture coordinates
static const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main()
{
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

// Fragment shader - sample from CEF texture
static const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D cefTexture;

void main()
{
    FragColor = texture(cefTexture, TexCoord);
}
)";

CEF_Drawer::CEF_Drawer()
	: window_(nullptr)
	, texture_id_(0)
	, vao_(0)
	, vbo_(0)
	, shader_program_(0)
	, width_(1920)
	, height_(1080)
	, initialized_(false)
	, browser_(nullptr)
	, url_("")
{
}

CEF_Drawer::~CEF_Drawer()
{
	shutdown();
}

bool CEF_Drawer::initialize(SDL_Window* window)
{
	if (initialized_)
	{
		std::cerr << "[CEF_Drawer] Already initialized" << std::endl;
		return false;
	}
	
	if (!window)
	{
		std::cerr << "[CEF_Drawer] Invalid SDL window" << std::endl;
		return false;
	}
	
	window_ = window;
	
	SDL_GetWindowSize(window_, &width_, &height_);
	
	glGenTextures(1, &texture_id_);
	glBindTexture(GL_TEXTURE_2D, texture_id_);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
	
	if (!createShaders())
	{
		std::cerr << "[CEF_Drawer] Failed to create shaders" << std::endl;
		shutdown();
		return false;
	}
	
	if (!createQuad())
	{
		std::cerr << "[CEF_Drawer] Failed to create quad" << std::endl;
		shutdown();
		return false;
	}
	
	initialized_ = true;
	std::cout << "[CEF_Drawer] Initialized (logical pixels): " << width_ << "x" << height_ << std::endl;
	
	return true;
}

bool CEF_Drawer::createBrowser(CefRefPtr<CefClient> client, const std::string& url, int width, int height)
{
	if (browser_)
	{
		browser_->GetHost()->CloseBrowser(true);
		browser_ = nullptr;
	}
	
	if (!initialized_)
	{
		std::cerr << "[CEF_Drawer] Cannot create browser: not initialized" << std::endl;
		return false;
	}
	
	if (browser_)
	{
		std::cerr << "[CEF_Drawer] Browser already created" << std::endl;
		return false;
	}
	
	url_ = url;
	width_ = width;
	height_ = height;
	
	glBindTexture(GL_TEXTURE_2D, texture_id_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
	
	CefBrowserSettings browser_settings;
	browser_settings.windowless_frame_rate = 60;
	
	CefWindowInfo window_info;
	window_info.SetAsWindowless(0);
	
	browser_ = CefBrowserHost::CreateBrowserSync(window_info, client, url_, browser_settings, nullptr, nullptr);
	
	if (!browser_)
	{
		std::cerr << "[CEF_Drawer] Failed to create CEF browser" << std::endl;
		return false;
	}
	
	std::cout << "[CEF_Drawer] Browser created with URL: " << url_ << " (logical pixels): " << width_ << "x" << height_ << std::endl;
	
	return true;
}

void CEF_Drawer::shutdown()
{
	if (vao_)
	{
		glDeleteVertexArrays(1, &vao_);
		vao_ = 0;
	}
	
	if (vbo_)
	{
		glDeleteBuffers(1, &vbo_);
		vbo_ = 0;
	}
	
	if (texture_id_)
	{
		glDeleteTextures(1, &texture_id_);
		texture_id_ = 0;
	}
	
	if (shader_program_)
	{
		glDeleteProgram(shader_program_);
		shader_program_ = 0;
	}
	
	initialized_ = false;
	std::cout << "[CEF_Drawer] Shutdown complete" << std::endl;
}

void CEF_Drawer::draw()
{
	if (!initialized_)
		return;
	
	if (window_)
	{
		int currentWidth, currentHeight;
		SDL_GetWindowSize(window_, &currentWidth, &currentHeight);
		
		if (currentWidth > 0 && currentHeight > 0 && (currentWidth != width_ || currentHeight != height_))
		{
			std::cout << "[CEF_Drawer] Auto-resize detected (logical pixels): " << width_ << "x" << height_ 
			          << " -> " << currentWidth << "x" << currentHeight << std::endl;
			
			width_ = currentWidth;
			height_ = currentHeight;
			
			glBindTexture(GL_TEXTURE_2D, texture_id_);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
			
			if (browser_)
			{
				CefRefPtr<CefBrowserHost> host = browser_->GetHost();
				if (host)
				{
					host->WasResized();
					host->Invalidate(PET_VIEW);
				}
			}
		}
	}
	
	std::lock_guard<std::mutex> lock(render_mutex_);
	
	glUseProgram(shader_program_);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_id_);
	
	glViewport(0, 0, width_, height_);
	
	glBindVertexArray(vao_);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
}

void CEF_Drawer::handleEvent(const SDL_Event& event)
{
	if (!browser_)
		return;
	
	CefRefPtr<CefBrowserHost> host = browser_->GetHost();
	if (!host)
		return;
	
	switch (event.type)
	{
		case SDL_EVENT_MOUSE_MOTION:
		{
			CefMouseEvent mouse_event;
			mouse_event.x = event.motion.x;
			mouse_event.y = event.motion.y;
			mouse_event.modifiers = GetCefModifiers(event);
			host->SendMouseMoveEvent(mouse_event, false);
			break;
		}
		
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		{
			CefMouseEvent mouse_event;
			mouse_event.x = event.button.x;
			mouse_event.y = event.button.y;
			mouse_event.modifiers = GetCefModifiers(event);
			
			CefBrowserHost::MouseButtonType button_type = MBT_LEFT;
			if (event.button.button == SDL_BUTTON_LEFT)
				button_type = MBT_LEFT;
			else if (event.button.button == SDL_BUTTON_RIGHT)
				button_type = MBT_RIGHT;
			else if (event.button.button == SDL_BUTTON_MIDDLE)
				button_type = MBT_MIDDLE;
			
			bool is_down = (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
			host->SendMouseClickEvent(mouse_event, button_type, !is_down, 1);
			
			// Send focus on mouse down
			if (is_down)
			{
				host->SetFocus(true);
			}
			break;
		}
		
		case SDL_EVENT_MOUSE_WHEEL:
		{
			CefMouseEvent mouse_event;
			// Use current mouse position (SDL doesn't provide it in wheel event)
			float mouseX, mouseY;
			SDL_GetMouseState(&mouseX, &mouseY);
			mouse_event.x = static_cast<int>(mouseX);
			mouse_event.y = static_cast<int>(mouseY);
			mouse_event.modifiers = GetCefModifiers(event);
			
			// SDL3 wheel values are float, convert to pixels (multiply by ~100 for smooth scrolling)
			int deltaX = static_cast<int>(event.wheel.x * 100.0f);
			int deltaY = static_cast<int>(event.wheel.y * 100.0f);
			
			host->SendMouseWheelEvent(mouse_event, deltaX, deltaY);
			break;
		}
		
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
		{
			CefKeyEvent key_event;
			key_event.windows_key_code = GetWindowsKeyCode(event.key.scancode, event.key.key);
			key_event.native_key_code = event.key.scancode;
			key_event.modifiers = GetCefKeyboardModifiers(event);
			
			if (event.type == SDL_EVENT_KEY_DOWN)
			{
				key_event.type = KEYEVENT_RAWKEYDOWN;
			}
			else
			{
				key_event.type = KEYEVENT_KEYUP;
			}
			
			host->SendKeyEvent(key_event);
			
			// For printable characters, send CHAR event
			if (event.type == SDL_EVENT_KEY_DOWN)
			{
				// Check if it's a printable character
				if (event.key.key >= 32 && event.key.key < 127)
				{
					CefKeyEvent char_event = key_event;
					char_event.type = KEYEVENT_CHAR;
					char_event.windows_key_code = event.key.key;
					host->SendKeyEvent(char_event);
				}
			}
			break;
		}
		
		case SDL_EVENT_TEXT_INPUT:
		{
			// Handle text input for complex input methods (IME, etc.)
			const char* text = event.text.text;
			for (const char* p = text; *p != 0; ++p)
			{
				CefKeyEvent key_event;
				key_event.type = KEYEVENT_CHAR;
				key_event.windows_key_code = *p;
				key_event.character = *p;
				key_event.unmodified_character = *p;
				key_event.modifiers = 0;
				host->SendKeyEvent(key_event);
			}
			break;
		}
		
		case SDL_EVENT_WINDOW_FOCUS_GAINED:
		{
			host->SetFocus(true);
			break;
		}
		
		case SDL_EVENT_WINDOW_FOCUS_LOST:
		{
			host->SetFocus(false);
			break;
		}
		
		case SDL_EVENT_WINDOW_MAXIMIZED:
		case SDL_EVENT_WINDOW_RESTORED:
		case SDL_EVENT_WINDOW_RESIZED:
		case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
		{
			int newWidth, newHeight;
			SDL_GetWindowSize(window_, &newWidth, &newHeight);
			
			std::cout << "[CEF_Drawer] Window event - current (logical pixels): " 
			          << width_ << "x" << height_ << ", new: " << newWidth << "x" << newHeight << std::endl;
			
			if (newWidth > 0 && newHeight > 0 && (newWidth != width_ || newHeight != height_))
			{
				width_ = newWidth;
				height_ = newHeight;
				
				glBindTexture(GL_TEXTURE_2D, texture_id_);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
				
				host->WasResized();
				host->Invalidate(PET_VIEW);
				
				std::cout << "[CEF_Drawer] Resized to " << width_ << "x" << height_ 
				          << " logical pixels and CEF invalidated" << std::endl;
			}
			break;
		}
	}
}

uint32_t CEF_Drawer::GetCefModifiers(const SDL_Event& event)
{
	uint32_t modifiers = 0;
	SDL_Keymod mod = SDL_GetModState();
	
	if (mod & SDL_KMOD_SHIFT)
		modifiers |= EVENTFLAG_SHIFT_DOWN;
	if (mod & SDL_KMOD_CTRL)
		modifiers |= EVENTFLAG_CONTROL_DOWN;
	if (mod & SDL_KMOD_ALT)
		modifiers |= EVENTFLAG_ALT_DOWN;
	if (mod & SDL_KMOD_GUI)
		modifiers |= EVENTFLAG_COMMAND_DOWN;
	
	// Mouse button modifiers
	if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || 
	    event.type == SDL_EVENT_MOUSE_BUTTON_UP ||
	    event.type == SDL_EVENT_MOUSE_MOTION)
	{
		Uint32 buttons = SDL_GetMouseState(nullptr, nullptr);
		if (buttons & SDL_BUTTON_LMASK)
			modifiers |= EVENTFLAG_LEFT_MOUSE_BUTTON;
		if (buttons & SDL_BUTTON_RMASK)
			modifiers |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
		if (buttons & SDL_BUTTON_MMASK)
			modifiers |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;
	}
	
	return modifiers;
}

uint32_t CEF_Drawer::GetCefKeyboardModifiers(const SDL_Event& event)
{
	uint32_t modifiers = 0;
	
	if (event.key.mod & SDL_KMOD_SHIFT)
		modifiers |= EVENTFLAG_SHIFT_DOWN;
	if (event.key.mod & SDL_KMOD_CTRL)
		modifiers |= EVENTFLAG_CONTROL_DOWN;
	if (event.key.mod & SDL_KMOD_ALT)
		modifiers |= EVENTFLAG_ALT_DOWN;
	if (event.key.mod & SDL_KMOD_GUI)
		modifiers |= EVENTFLAG_COMMAND_DOWN;
	if (event.key.mod & SDL_KMOD_NUM)
		modifiers |= EVENTFLAG_NUM_LOCK_ON;
	if (event.key.mod & SDL_KMOD_CAPS)
		modifiers |= EVENTFLAG_CAPS_LOCK_ON;
	
	return modifiers;
}

int CEF_Drawer::GetWindowsKeyCode(SDL_Scancode scancode, SDL_Keycode key)
{
	// Map SDL scancodes to Windows virtual key codes
	// This is a simplified mapping - add more as needed
	switch (scancode)
	{
		case SDL_SCANCODE_BACKSPACE: return 0x08; // VK_BACK
		case SDL_SCANCODE_TAB: return 0x09; // VK_TAB
		case SDL_SCANCODE_RETURN: return 0x0D; // VK_RETURN
		case SDL_SCANCODE_ESCAPE: return 0x1B; // VK_ESCAPE
		case SDL_SCANCODE_SPACE: return 0x20; // VK_SPACE
		case SDL_SCANCODE_DELETE: return 0x2E; // VK_DELETE
		case SDL_SCANCODE_LEFT: return 0x25; // VK_LEFT
		case SDL_SCANCODE_UP: return 0x26; // VK_UP
		case SDL_SCANCODE_RIGHT: return 0x27; // VK_RIGHT
		case SDL_SCANCODE_DOWN: return 0x28; // VK_DOWN
		case SDL_SCANCODE_HOME: return 0x24; // VK_HOME
		case SDL_SCANCODE_END: return 0x23; // VK_END
		case SDL_SCANCODE_PAGEUP: return 0x21; // VK_PRIOR
		case SDL_SCANCODE_PAGEDOWN: return 0x22; // VK_NEXT
		case SDL_SCANCODE_LSHIFT: return 0xA0; // VK_LSHIFT
		case SDL_SCANCODE_RSHIFT: return 0xA1; // VK_RSHIFT
		case SDL_SCANCODE_LCTRL: return 0xA2; // VK_LCONTROL
		case SDL_SCANCODE_RCTRL: return 0xA3; // VK_RCONTROL
		case SDL_SCANCODE_LALT: return 0xA4; // VK_LMENU
		case SDL_SCANCODE_RALT: return 0xA5; // VK_RMENU
		
		// F keys
		case SDL_SCANCODE_F1: return 0x70; // VK_F1
		case SDL_SCANCODE_F2: return 0x71;
		case SDL_SCANCODE_F3: return 0x72;
		case SDL_SCANCODE_F4: return 0x73;
		case SDL_SCANCODE_F5: return 0x74;
		case SDL_SCANCODE_F6: return 0x75;
		case SDL_SCANCODE_F7: return 0x76;
		case SDL_SCANCODE_F8: return 0x77;
		case SDL_SCANCODE_F9: return 0x78;
		case SDL_SCANCODE_F10: return 0x79;
		case SDL_SCANCODE_F11: return 0x7A;
		case SDL_SCANCODE_F12: return 0x7B;
		
		default:
			// For alphanumeric keys, use the SDL keycode
			if (key >= 32 && key < 127)
			{
				return toupper(key);
			}
			return 0;
	}
}

void CEF_Drawer::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect)
{
	rect.x = 0;
	rect.y = 0;
	rect.width = width_;
	rect.height = height_;
	std::cout << "[CEF_Drawer] GetViewRect called: " << width_ << "x" << height_ << std::endl;
}

void CEF_Drawer::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
                         const RectList& dirtyRects, const void* buffer,
                         int width, int height)
{
	if (type != PET_VIEW)
		return;
	
	std::lock_guard<std::mutex> lock(render_mutex_);
	
	if (width != width_ || height != height_)
	{
		std::cout << "[CEF_Drawer] OnPaint size mismatch: expected " << width_ << "x" << height_ 
		          << ", got " << width << "x" << height << std::endl;
		
		width_ = width;
		height_ = height;
		
		glBindTexture(GL_TEXTURE_2D, texture_id_);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
	}
	
	updateTexture(buffer, width, height);
}

void CEF_Drawer::resize(int width, int height)
{
	std::lock_guard<std::mutex> lock(render_mutex_);
	
	width_ = width;
	height_ = height;
	
	glBindTexture(GL_TEXTURE_2D, texture_id_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
	
	if (browser_)
	{
		CefRefPtr<CefBrowserHost> host = browser_->GetHost();
		if (host)
		{
			host->WasResized();
			host->Invalidate(PET_VIEW);
		}
	}
	
	std::cout << "[CEF_Drawer] Resized (logical pixels): " << width << "x" << height << std::endl;
}

bool CEF_Drawer::createShaders()
{
	// Compile vertex shader
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
	glCompileShader(vertexShader);
	
	GLint success;
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		char infoLog[512];
		glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
		std::cerr << "[CEF_Drawer] Vertex shader compilation failed: " << infoLog << std::endl;
		glDeleteShader(vertexShader);
		return false;
	}
	
	// Compile fragment shader
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
	glCompileShader(fragmentShader);
	
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		char infoLog[512];
		glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
		std::cerr << "[CEF_Drawer] Fragment shader compilation failed: " << infoLog << std::endl;
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
		return false;
	}
	
	// Link shader program
	shader_program_ = glCreateProgram();
	glAttachShader(shader_program_, vertexShader);
	glAttachShader(shader_program_, fragmentShader);
	glLinkProgram(shader_program_);
	
	glGetProgramiv(shader_program_, GL_LINK_STATUS, &success);
	if (!success)
	{
		char infoLog[512];
		glGetProgramInfoLog(shader_program_, 512, nullptr, infoLog);
		std::cerr << "[CEF_Drawer] Shader program linking failed: " << infoLog << std::endl;
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);
		glDeleteProgram(shader_program_);
		shader_program_ = 0;
		return false;
	}
	
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	
	// Set texture uniform
	glUseProgram(shader_program_);
	glUniform1i(glGetUniformLocation(shader_program_, "cefTexture"), 0);
	
	return true;
}

bool CEF_Drawer::createQuad()
{
	// Fullscreen quad vertices (position + texcoord)
	float vertices[] = {
		// Positions   // TexCoords
		-1.0f,  1.0f,  0.0f, 0.0f,  // Top-left
		-1.0f, -1.0f,  0.0f, 1.0f,  // Bottom-left
		 1.0f, -1.0f,  1.0f, 1.0f,  // Bottom-right
		
		-1.0f,  1.0f,  0.0f, 0.0f,  // Top-left
		 1.0f, -1.0f,  1.0f, 1.0f,  // Bottom-right
		 1.0f,  1.0f,  1.0f, 0.0f   // Top-right
	};
	
	glGenVertexArrays(1, &vao_);
	glGenBuffers(1, &vbo_);
	
	glBindVertexArray(vao_);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	
	// Position attribute
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	
	// TexCoord attribute
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);
	
	glBindVertexArray(0);
	
	return true;
}

void CEF_Drawer::updateTexture(const void* buffer, int width, int height)
{
	if (!buffer || !texture_id_)
		return;
	
	glBindTexture(GL_TEXTURE_2D, texture_id_);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGRA, GL_UNSIGNED_BYTE, buffer);
}
