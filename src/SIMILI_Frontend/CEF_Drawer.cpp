#include "CEF_Drawer.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <sstream>
#include <string>
#include "include/base/cef_callback.h"
#include "include/cef_browser.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

CEF_Drawer* CEF_Drawer::active_instance_ = nullptr;

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
	, logical_width_(0)
	, logical_height_(0)
	, drawable_width_(0)
	, drawable_height_(0)
	, dpi_scale_(1.0f)
	, initialized_(false)
	, browser_(nullptr)
	, url_("")
	, paint_buffer_width_(0)
	, paint_buffer_height_(0)
	, runtime_layout_sync_pending_(false)
	, runtime_layout_waiting_for_paint_(false)
	, runtime_layout_needs_second_invalidate_(false)
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
	updateWindowProperties();
	width_ = logical_width_;
	height_ = logical_height_;
	
	glGenTextures(1, &texture_id_);
	glBindTexture(GL_TEXTURE_2D, texture_id_);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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
	active_instance_ = this;
	std::cout << "[CEF_Drawer] Initialized (logical pixels): " << width_ << "x" << height_ << std::endl;
	
	return true;
}

void CEF_Drawer::syncWindowProperties()
{
	if (!initialized_ || !window_)
	{
		return;
	}

	updateWindowProperties();

	int newWidth = logical_width_;
	int newHeight = logical_height_;

	if (newWidth <= 0 || newHeight <= 0 || (newWidth == width_ && newHeight == height_))
	{
		return;
	}

	{
		std::lock_guard<std::mutex> lock(render_mutex_);
		width_ = newWidth;
		height_ = newHeight;
		ensureTextureStorage(width_, height_);
	}

	if (browser_)
	{
		CefRefPtr<CefBrowserHost> host = browser_->GetHost();
		if (host)
		{
			host->WasResized();
			host->Invalidate(PET_VIEW);
		}
	}

	std::cout << "[CEF_Drawer] Synced SDL window properties: " << width_ << "x" << height_ << std::endl;
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
	updateWindowProperties();
	
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
	{
		std::lock_guard<std::mutex> lock(render_mutex_);
		ui_panel_frames_.clear();
		ui_panel_display_frames_.clear();
		runtime_layout_frames_.clear();
		paint_buffer_.clear();
		paint_buffer_width_ = 0;
		paint_buffer_height_ = 0;
		runtime_layout_sync_pending_ = false;
		runtime_layout_waiting_for_paint_ = false;
		for (auto& texturePair : ui_panel_textures_)
		{
			if (texturePair.second.texture_id != 0)
			{
				glDeleteTextures(1, &texturePair.second.texture_id);
				texturePair.second.texture_id = 0;
			}
		}
		ui_panel_textures_.clear();
		if (active_instance_ == this)
		{
			active_instance_ = nullptr;
		}
	}

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
	syncWindowProperties();
}

void CEF_Drawer::updateUIPanelFrames(const std::map<std::string, UIPanelFrameData>& panelFrames)
{
	std::lock_guard<std::mutex> lock(render_mutex_);
	ui_panel_frames_ = panelFrames;

	for (auto& texturePair : ui_panel_textures_)
	{
		texturePair.second.dirty = true;
	}

	for (auto it = ui_panel_display_frames_.begin(); it != ui_panel_display_frames_.end();)
	{
		if (ui_panel_frames_.find(it->first) == ui_panel_frames_.end())
		{
			it = ui_panel_display_frames_.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void CEF_Drawer::updateUIPanelDisplayFrame(const std::string& panelName, const UIPanelFrameData& panelFrame)
{
	std::lock_guard<std::mutex> lock(render_mutex_);
	ui_panel_display_frames_[panelName] = panelFrame;
	// Do NOT sync to ui_panel_frames_ - display uses SDL drawable coords, frames use CEF logical coords
}

void CEF_Drawer::updateUIPanelSourceFrame(const std::string& panelName, const UIPanelFrameData& sourceFrame)
{
	std::lock_guard<std::mutex> lock(render_mutex_);
	ui_panel_frames_[panelName] = sourceFrame;
}

bool CEF_Drawer::getUIPanelTextureRegion(const std::string& panelName, GLuint& outTextureId, int& outTextureWidth, int& outTextureHeight, UIPanelFrameData& outFrame)
{
	std::lock_guard<std::mutex> lock(render_mutex_);

	auto it = ui_panel_frames_.find(panelName);
	if (it == ui_panel_frames_.end())
	{
		return false;
	}

	if (it->second.width <= 0 || it->second.height <= 0)
	{
		return false;
	}

	UIPanelTextureData& textureData = ui_panel_textures_[panelName];
	if (!rebuildUIPanelTextureLocked(panelName, textureData, outFrame))
	{
		return false;
	}

	outTextureId = textureData.texture_id;
	outTextureWidth = textureData.width;
	outTextureHeight = textureData.height;
	return true;
}

bool CEF_Drawer::getActiveUIPanelTextureRegion(const std::string& panelName, GLuint& outTextureId, int& outTextureWidth, int& outTextureHeight, UIPanelFrameData& outFrame)
{
	if (!active_instance_)
	{
		return false;
	}

	return active_instance_->getUIPanelTextureRegion(panelName, outTextureId, outTextureWidth, outTextureHeight, outFrame);
}

bool CEF_Drawer::isUIPanelTextureDirty(const std::string& panelName)
{
	std::lock_guard<std::mutex> lock(render_mutex_);

	auto it = ui_panel_textures_.find(panelName);
	if (it == ui_panel_textures_.end())
	{
		return true;
	}

	return it->second.dirty;
}

bool CEF_Drawer::isActiveUIPanelTextureDirty(const std::string& panelName)
{
	if (!active_instance_)
	{
		return false;
	}

	return active_instance_->isUIPanelTextureDirty(panelName);
}

void CEF_Drawer::updateActiveUIPanelDisplayFrame(const std::string& panelName, const UIPanelFrameData& panelFrame)
{
	if (!active_instance_)
	{
		return;
	}

	active_instance_->updateUIPanelDisplayFrame(panelName, panelFrame);
}

void CEF_Drawer::updateActiveUIPanelSourceFrame(const std::string& panelName, const UIPanelFrameData& sourceFrame)
{
	if (!active_instance_)
	{
		return;
	}

	active_instance_->updateUIPanelSourceFrame(panelName, sourceFrame);
}

void CEF_Drawer::requestActiveRuntimeLayoutSync(const std::map<std::string, UIPanelFrameData>& panelFrames)
{
	if (!active_instance_)
	{
		return;
	}

	active_instance_->requestRuntimeLayoutSync(panelFrames);
}

void CEF_Drawer::forceActiveLayoutSync()
{
	if (!active_instance_)
	{
		return;
	}

	active_instance_->forceLayoutSync();
}

void CEF_Drawer::requestRuntimeLayoutSync(const std::map<std::string, UIPanelFrameData>& panelFrames)
{
	if (panelFrames.empty())
	{
		return;
	}

	bool shouldSchedule = false;
	{
		std::lock_guard<std::mutex> lock(render_mutex_);
		runtime_layout_frames_ = panelFrames;
		runtime_layout_waiting_for_paint_ = true;
		runtime_layout_needs_second_invalidate_ = true;

		if (!runtime_layout_sync_pending_)
		{
			runtime_layout_sync_pending_ = true;
			shouldSchedule = true;
		}
	}

	if (!shouldSchedule)
	{
		return;
	}

	if (CefCurrentlyOn(TID_UI))
	{
		flushRuntimeLayoutSync();
		return;
	}

	CefPostTask(TID_UI, base::BindOnce(&CEF_Drawer::flushRuntimeLayoutSync, base::Unretained(this)));
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
			translateMousePosition(event.motion.x, event.motion.y, mouse_event.x, mouse_event.y);
			mouse_event.modifiers = GetCefModifiers(event);
			host->SendMouseMoveEvent(mouse_event, false);
			break;
		}
		
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
		{
			CefMouseEvent mouse_event;
			translateMousePosition(event.button.x, event.button.y, mouse_event.x, mouse_event.y);
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
			translateMousePosition(mouseX, mouseY, mouse_event.x, mouse_event.y);
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
			syncWindowProperties();
			break;
		}
	}
}

CEF_Drawer::SDLWindowProperties CEF_Drawer::getSDLWindowProperties()
{
	std::lock_guard<std::mutex> lock(render_mutex_);

	SDLWindowProperties properties;
	properties.logical_width = logical_width_;
	properties.logical_height = logical_height_;
	properties.drawable_width = drawable_width_;
	properties.drawable_height = drawable_height_;
	properties.dpi_scale = dpi_scale_;
	return properties;
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
const RectList& dirtyRects, const void* buffer, int width, int height)
{
	if (type != PET_VIEW)
		return;
	
	bool needsSecondInvalidate = false;
	CefRefPtr<CefBrowser> browserRef;
	
	{
		std::lock_guard<std::mutex> lock(render_mutex_);
		
		if (width != width_ || height != height_)
		{
			
			width_ = width;
			height_ = height;
			ensureTextureStorage(width_, height_);
		}

		std::size_t bufferSize = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
		paint_buffer_width_ = width;
		paint_buffer_height_ = height;
		paint_buffer_.resize(bufferSize);
		if (buffer && bufferSize > 0)
		{
			std::memcpy(paint_buffer_.data(), buffer, bufferSize);
		}

		if (runtime_layout_waiting_for_paint_ && !runtime_layout_frames_.empty())
		{
			for (const auto& pair : runtime_layout_frames_)
			{
				if (pair.first != "viewport_panel")
				{
					std::cout << "  " << pair.first << ": " << pair.second.width << "x" << pair.second.height 
					          << " at (" << pair.second.x << "," << pair.second.y << ")" << std::endl;
					ui_panel_frames_[pair.first] = pair.second;
					// Synchronize display frames immediately to ensure mouse events work
					ui_panel_display_frames_[pair.first] = pair.second;
				}
			}
			needsSecondInvalidate = runtime_layout_needs_second_invalidate_;
			runtime_layout_needs_second_invalidate_ = false;
		}

		runtime_layout_waiting_for_paint_ = false;

		for (auto& texturePair : ui_panel_textures_)
		{
			texturePair.second.dirty = false;
		}

		bool markedDirtyPanel = false;
		for (auto& texturePair : ui_panel_textures_)
		{
			auto sourceIt = ui_panel_frames_.find(texturePair.first);
			if (sourceIt == ui_panel_frames_.end())
			{
				continue;
			}

			const UIPanelFrameData& sourceFrame = sourceIt->second;
			if (sourceFrame.width <= 0 || sourceFrame.height <= 0)
			{
				continue;
			}

			for (const CefRect& dirtyRect : dirtyRects)
			{
				const int dirtyMinX = (std::max)(dirtyRect.x, sourceFrame.x);
				const int dirtyMinY = (std::max)(dirtyRect.y, sourceFrame.y);
				const int dirtyMaxX = (std::min)(dirtyRect.x + dirtyRect.width, sourceFrame.x + sourceFrame.width);
				const int dirtyMaxY = (std::min)(dirtyRect.y + dirtyRect.height, sourceFrame.y + sourceFrame.height);

				if (dirtyMinX < dirtyMaxX && dirtyMinY < dirtyMaxY)
				{
					texturePair.second.dirty = true;
					markedDirtyPanel = true;
					break;
				}
			}
		}

		if (!markedDirtyPanel && dirtyRects.empty())
		{
			for (auto& texturePair : ui_panel_textures_)
			{
				texturePair.second.dirty = true;
			}
		}

		updateTexture(buffer, width, height);
		
		// Capture browser reference before releasing mutex
		browserRef = browser_;
	} // Mutex released here


	if (needsSecondInvalidate && browserRef)
	{
		std::cout << "[CEF_Drawer::OnPaint] Scheduling second invalidation to refresh interactive zones" << std::endl;
		CefPostTask(TID_UI, base::BindOnce([](CefRefPtr<CefBrowser> browser) {
			if (browser)
			{
				CefRefPtr<CefBrowserHost> host = browser->GetHost();
				if (host)
				{
					host->WasResized();
					host->Invalidate(PET_VIEW);
				}
			}
		}, browserRef));
	}
}

void CEF_Drawer::resize(int width, int height)
{
	{
		std::lock_guard<std::mutex> lock(render_mutex_);
		width_ = width;
		height_ = height;
		updateWindowProperties();
		ensureTextureStorage(width_, height_);
	}
	
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

void CEF_Drawer::ensureTextureStorage(int width, int height)
{
	if (!texture_id_ || width <= 0 || height <= 0)
	{
		return;
	}

	glBindTexture(GL_TEXTURE_2D, texture_id_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
}

bool CEF_Drawer::rebuildUIPanelTextureLocked(const std::string& panelName, UIPanelTextureData& textureData, UIPanelFrameData& outFrame)
{
	auto sourceIt = ui_panel_frames_.find(panelName);
	if (sourceIt == ui_panel_frames_.end())
	{
		return false;
	}

	UIPanelFrameData sourceFrame = sourceIt->second;
	UIPanelFrameData displayFrame = sourceFrame;

	auto displayIt = ui_panel_display_frames_.find(panelName);
	if (displayIt != ui_panel_display_frames_.end())
	{
		displayFrame = displayIt->second;
	}

	if (sourceFrame.width <= 0 || sourceFrame.height <= 0 || displayFrame.width <= 0 || displayFrame.height <= 0)
	{
		return false;
	}

	if (paint_buffer_.empty() || paint_buffer_width_ <= 0 || paint_buffer_height_ <= 0)
	{
		return false;
	}

	int sourceMinX = std::clamp(sourceFrame.x, 0, paint_buffer_width_ - 1);
	int sourceMinY = std::clamp(sourceFrame.y, 0, paint_buffer_height_ - 1);
	int sourceMaxX = std::clamp(sourceFrame.x + sourceFrame.width, sourceMinX + 1, paint_buffer_width_);
	int sourceMaxY = std::clamp(sourceFrame.y + sourceFrame.height, sourceMinY + 1, paint_buffer_height_);
	int sourceWidth = sourceMaxX - sourceMinX;
	int sourceHeight = sourceMaxY - sourceMinY;

	if (sourceWidth <= 0 || sourceHeight <= 0)
	{
		return false;
	}

	int targetWidth = sourceWidth > 0 ? sourceWidth : 1;
	int targetHeight = sourceHeight > 0 ? sourceHeight : 1;

	bool needsRebuild = textureData.dirty || textureData.texture_id == 0 || textureData.width != targetWidth || textureData.height != targetHeight;
	if (!needsRebuild)
	{
		outFrame.x = 0;
		outFrame.y = 0;
		outFrame.width = textureData.width;
		outFrame.height = textureData.height;
		return true;
	}


	std::vector<unsigned char> panelPixels(static_cast<std::size_t>(targetWidth) * static_cast<std::size_t>(targetHeight) * 4u);

	for (int targetY = 0; targetY < targetHeight; ++targetY)
	{
		float normalizedY = (static_cast<float>(targetY) + 0.5f) / static_cast<float>(targetHeight);
		int sampleY = sourceMinY + static_cast<int>(std::floor(normalizedY * static_cast<float>(sourceHeight)));
		sampleY = std::clamp(sampleY, sourceMinY, sourceMaxY - 1);

		for (int targetX = 0; targetX < targetWidth; ++targetX)
		{
			float normalizedX = (static_cast<float>(targetX) + 0.5f) / static_cast<float>(targetWidth);
			int sampleX = sourceMinX + static_cast<int>(std::floor(normalizedX * static_cast<float>(sourceWidth)));
			sampleX = std::clamp(sampleX, sourceMinX, sourceMaxX - 1);

			std::size_t sourceIndex = (static_cast<std::size_t>(sampleY) * static_cast<std::size_t>(paint_buffer_width_) + static_cast<std::size_t>(sampleX)) * 4u;
			std::size_t targetIndex = (static_cast<std::size_t>(targetY) * static_cast<std::size_t>(targetWidth) + static_cast<std::size_t>(targetX)) * 4u;

			panelPixels[targetIndex + 0] = paint_buffer_[sourceIndex + 0];
			panelPixels[targetIndex + 1] = paint_buffer_[sourceIndex + 1];
			panelPixels[targetIndex + 2] = paint_buffer_[sourceIndex + 2];
			panelPixels[targetIndex + 3] = paint_buffer_[sourceIndex + 3];
		}
	}

	if (textureData.texture_id == 0)
	{
		glGenTextures(1, &textureData.texture_id);
		glBindTexture(GL_TEXTURE_2D, textureData.texture_id);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, textureData.texture_id);
	}

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, targetWidth, targetHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, panelPixels.data());

	textureData.width = targetWidth;
	textureData.height = targetHeight;
	textureData.dirty = false;

	outFrame.x = 0;
	outFrame.y = 0;
	outFrame.width = targetWidth;
	outFrame.height = targetHeight;
	return true;
}

bool CEF_Drawer::translateMousePosition(float inputX, float inputY, int& outputX, int& outputY)
{
	std::lock_guard<std::mutex> lock(render_mutex_);

	outputX = static_cast<int>(std::lround(inputX));
	outputY = static_cast<int>(std::lround(inputY));

	for (const auto& displayPair : ui_panel_display_frames_)
	{
		auto sourceIt = ui_panel_frames_.find(displayPair.first);
		if (sourceIt == ui_panel_frames_.end())
		{
			continue;
		}

		const UIPanelFrameData& displayFrame = displayPair.second;
		const UIPanelFrameData& sourceFrame = sourceIt->second;

		if (displayFrame.width <= 0 || displayFrame.height <= 0 || sourceFrame.width <= 0 || sourceFrame.height <= 0)
		{
			continue;
		}

		float minX = static_cast<float>(displayFrame.x);
		float minY = static_cast<float>(displayFrame.y);
		float maxX = minX + static_cast<float>(displayFrame.width);
		float maxY = minY + static_cast<float>(displayFrame.height);

		if (inputX < minX || inputX >= maxX || inputY < minY || inputY >= maxY)
		{
			continue;
		}

		float normalizedX = (inputX - minX) / static_cast<float>(displayFrame.width);
		float normalizedY = (inputY - minY) / static_cast<float>(displayFrame.height);

		normalizedX = std::clamp(normalizedX, 0.0f, 1.0f);
		normalizedY = std::clamp(normalizedY, 0.0f, 1.0f);

		float mappedX = static_cast<float>(sourceFrame.x) + normalizedX * static_cast<float>(sourceFrame.width);
		float mappedY = static_cast<float>(sourceFrame.y) + normalizedY * static_cast<float>(sourceFrame.height);

		outputX = static_cast<int>(std::lround(mappedX));
		outputY = static_cast<int>(std::lround(mappedY));
		return true;
	}

	return false;
}

void CEF_Drawer::updateWindowProperties()
{
	if (!window_)
	{
		logical_width_ = 0;
		logical_height_ = 0;
		drawable_width_ = 0;
		drawable_height_ = 0;
		dpi_scale_ = 1.0f;
		return;
	}

	SDL_GetWindowSize(window_, &logical_width_, &logical_height_);
	SDL_GetWindowSizeInPixels(window_, &drawable_width_, &drawable_height_);

	if (logical_width_ > 0 && logical_height_ > 0 && drawable_width_ > 0 && drawable_height_ > 0)
	{
		dpi_scale_ = static_cast<float>(drawable_width_) / static_cast<float>(logical_width_);
	}
	else
	{
		dpi_scale_ = 1.0f;
	}
}

bool CEF_Drawer::createShaders()
{
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

void CEF_Drawer::flushRuntimeLayoutSync()
{
	std::map<std::string, UIPanelFrameData> panelFrames;
	CefRefPtr<CefBrowser> browser;
	{
		std::lock_guard<std::mutex> lock(render_mutex_);
		runtime_layout_sync_pending_ = false;
		panelFrames = runtime_layout_frames_;
		browser = browser_;
	}

	if (!browser || panelFrames.empty())
	{
		return;
	}

	CefRefPtr<CefFrame> mainFrame = browser->GetMainFrame();
	if (!mainFrame || !mainFrame->IsValid())
	{
		return;
	}

	const std::string script = buildRuntimeLayoutSyncScript(panelFrames);
	if (script.empty())
	{
		return;
	}

	mainFrame->ExecuteJavaScript(script, mainFrame->GetURL(), 0);

	CefRefPtr<CefBrowserHost> host = browser->GetHost();
	if (host)
	{
		host->Invalidate(PET_VIEW);
	}
}

std::string CEF_Drawer::buildRuntimeLayoutSyncScript(const std::map<std::string, UIPanelFrameData>& panelFrames) const
{
	if (panelFrames.empty())
	{
		return std::string();
	}

	std::ostringstream script;
	script << "(function(){";
	script << "const frameMap={";
	bool firstFrame = true;
	for (const auto& pair : panelFrames)
	{
		if (!firstFrame)
		{
			script << ",";
		}

		firstFrame = false;
		script << "'" << pair.first << "':{";
		script << "x:" << pair.second.x << ",";
		script << "y:" << pair.second.y << ",";
		script << "width:" << pair.second.width << ",";
		script << "height:" << pair.second.height;
		script << "}";
	}
	script << "};";
	script
		<< "const container=document.querySelector('.main-container');"
		<< "const leftSection=document.querySelector('.left-section');"
		<< "const centerSection=document.querySelector('.center-section');"
		<< "const rightSection=document.querySelector('.right-section');"
		<< "const topRow=document.querySelector('.top-row');"
		<< "const projectViewerPanel=document.querySelector('.project-viewer-panel');"
		<< "if(!container||!leftSection||!centerSection||!rightSection||!topRow||!projectViewerPanel){return;}"
		<< "const sectionWidths={};"
		<< "let topRowTop=null;"
		<< "let topRowBottom=null;"
		<< "const getPanelName=function(iframe){"
		<< "if(!iframe||!iframe.parentElement){return null;}"
		<< "const panelClasses=iframe.parentElement.classList;"
		<< "for(let index=0;index<panelClasses.length;++index){"
		<< "const className=panelClasses[index];"
		<< "if(!className||className==='panel'){continue;}"
		<< "return className.replace(/-/g,'_');"
		<< "}"
		<< "return null;"
		<< "};"
		<< "container.querySelectorAll('iframe').forEach(function(iframe){"
		<< "const panelName=getPanelName(iframe);"
		<< "if(!panelName){return;}"
		<< "const frame=frameMap[panelName];"
		<< "if(!frame||frame.width<=0||frame.height<=0){return;}"
		<< "const panelElement=iframe.parentElement;"
		<< "const sectionElement=panelElement?panelElement.parentElement:null;"
		<< "if(!panelElement){return;}"
		<< "if(panelElement.classList.contains('project-viewer-panel')){"
		<< "panelElement.style.flex='0 0 auto';"
		<< "panelElement.style.height=frame.height+'px';"
		<< "return;"
		<< "}"
		<< "if(!sectionElement||!sectionElement.classList){return;}"
		<< "let sectionKey='';"
		<< "if(sectionElement.classList.contains('left-section')){sectionKey='left-section';}"
		<< "else if(sectionElement.classList.contains('center-section')){sectionKey='center-section';}"
		<< "else if(sectionElement.classList.contains('right-section')){sectionKey='right-section';}"
		<< "if(!sectionKey){return;}"
		<< "topRowTop=topRowTop===null?frame.y:Math.min(topRowTop,frame.y);"
		<< "topRowBottom=topRowBottom===null?(frame.y+frame.height):Math.max(topRowBottom,frame.y+frame.height);"
		<< "sectionElement.style.flex='0 0 auto';"
		<< "sectionWidths[sectionKey]=sectionWidths[sectionKey]?Math.max(sectionWidths[sectionKey],frame.width):frame.width;"
		<< "if(sectionKey==='right-section'){"
		<< "panelElement.style.flex='0 0 auto';"
		<< "panelElement.style.height=frame.height+'px';"
		<< "}else{"
		<< "panelElement.style.height='';"
		<< "panelElement.style.flex='';"
		<< "}"
		<< "});"
		<< "leftSection.style.flex='0 0 auto';"
		<< "centerSection.style.flex='0 0 auto';"
		<< "rightSection.style.flex='0 0 auto';"
		<< "projectViewerPanel.style.flex='0 0 auto';"
		<< "if(sectionWidths['left-section']){leftSection.style.width=sectionWidths['left-section']+'px';}"
		<< "if(sectionWidths['center-section']){centerSection.style.width=sectionWidths['center-section']+'px';}"
		<< "if(sectionWidths['right-section']){rightSection.style.width=sectionWidths['right-section']+'px';}"
		<< "if(topRowTop!==null&&topRowBottom!==null){topRow.style.flex='0 0 auto';topRow.style.height=(topRowBottom-topRowTop)+'px';}"
		<< "if(leftSection.firstElementChild){leftSection.firstElementChild.style.height='';leftSection.firstElementChild.style.flex='';}"
		<< "if(centerSection.firstElementChild){centerSection.firstElementChild.style.height='';centerSection.firstElementChild.style.flex='';}"
		<< "document.body.offsetHeight;"
		<< "})();";

	return script.str();
}

void CEF_Drawer::forceLayoutSync()
{
	if (!browser_)
	{
		return;
	}

	{
		std::lock_guard<std::mutex> lock(render_mutex_);
		runtime_layout_waiting_for_paint_ = true;
		for (auto& texturePair : ui_panel_textures_)
		{
			texturePair.second.dirty = true;
		}
	}

	CefRefPtr<CefBrowserHost> host = browser_->GetHost();
	if (host)
	{
		host->WasResized();
		host->Invalidate(PET_VIEW);
	}
}
