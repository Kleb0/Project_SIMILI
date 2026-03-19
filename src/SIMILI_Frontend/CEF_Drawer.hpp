#pragma once

#include "include/cef_render_handler.h"
#include "include/cef_client.h"
#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <map>
#include <mutex>
#include <vector>
#include <iostream>

/**
 * @brief Handles CEF off-screen rendering and draws it into SDL window
 * 
 * This class implements CefRenderHandler to receive rendered frames from CEF
 * in off-screen (windowless) mode, and provides methods to draw those frames
 * into an SDL/OpenGL context.
 */
class CEF_Drawer : public CefRenderHandler
{
public:
	struct SDLWindowProperties
	{
		int logical_width;
		int logical_height;
		int drawable_width;
		int drawable_height;
		float dpi_scale;
	};

	struct UIPanelFrameData
	{
		int x;
		int y;
		int width;
		int height;
	};

	struct UIPanelTextureData
	{
		GLuint texture_id = 0;
		int width = 0;
		int height = 0;
		bool dirty = true;
	};

	CEF_Drawer();
	virtual ~CEF_Drawer();
	
	// Initialize OpenGL resources for drawing CEF content
	bool initialize(SDL_Window* window);
	void shutdown();
	void syncWindowProperties();
	
	// Create CEF browser with specified URL
	bool createBrowser(CefRefPtr<CefClient> client, const std::string& url, int width, int height);
	
	// Draw the current CEF frame to the OpenGL context
	void draw();
	
	// Handle SDL events and forward them to CEF browser
	void handleEvent(const SDL_Event& event);
	
	// CefRenderHandler interface
	void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
	void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
	             const RectList& dirtyRects, const void* buffer,
	             int width, int height) override;
	
	// Resize handling
	void resize(int width, int height);
	void updateUIPanelFrames(const std::map<std::string, UIPanelFrameData>& panelFrames);
	void updateUIPanelDisplayFrame(const std::string& panelName, const UIPanelFrameData& panelFrame);
	void updateUIPanelSourceFrame(const std::string& panelName, const UIPanelFrameData& sourceFrame);
	bool getUIPanelTextureRegion(const std::string& panelName, GLuint& outTextureId, int& outTextureWidth, int& outTextureHeight, UIPanelFrameData& outFrame);
	bool isUIPanelTextureDirty(const std::string& panelName);
	static bool getActiveUIPanelTextureRegion(const std::string& panelName, GLuint& outTextureId, int& outTextureWidth, int& outTextureHeight, UIPanelFrameData& outFrame);
	static bool isActiveUIPanelTextureDirty(const std::string& panelName);
	static void updateActiveUIPanelDisplayFrame(const std::string& panelName, const UIPanelFrameData& panelFrame);
	static void updateActiveUIPanelSourceFrame(const std::string& panelName, const UIPanelFrameData& sourceFrame);
	static void requestActiveRuntimeLayoutSync(const std::map<std::string, UIPanelFrameData>& panelFrames);
	static void forceActiveLayoutSync();
	void requestRuntimeLayoutSync(const std::map<std::string, UIPanelFrameData>& panelFrames);
	void forceLayoutSync();
	
	// Accessors
	int getWidth() const { return width_; }
	int getHeight() const { return height_; }
	bool isInitialized() const { return initialized_; }
	SDL_Window* getWindowHandle() const { return window_; }
	SDLWindowProperties getSDLWindowProperties();
	
private:
	SDL_Window* window_;
	GLuint texture_id_;
	GLuint vao_;
	GLuint vbo_;
	GLuint shader_program_;
	
	int width_;
	int height_;
	int logical_width_;
	int logical_height_;
	int drawable_width_;
	int drawable_height_;
	float dpi_scale_;
	bool initialized_;
	
	CefRefPtr<CefBrowser> browser_;
	std::string url_;
	std::map<std::string, UIPanelFrameData> ui_panel_frames_;
	std::map<std::string, UIPanelFrameData> ui_panel_display_frames_;
	std::map<std::string, UIPanelFrameData> runtime_layout_frames_;
	std::map<std::string, UIPanelTextureData> ui_panel_textures_;
	std::vector<unsigned char> paint_buffer_;
	int paint_buffer_width_;
	int paint_buffer_height_;
	bool runtime_layout_sync_pending_;
	bool runtime_layout_waiting_for_paint_;
	bool runtime_layout_needs_second_invalidate_;
	
	std::mutex render_mutex_;
	static CEF_Drawer* active_instance_;
	
	// OpenGL setup helpers
	bool createShaders();
	bool createQuad();
	void ensureTextureStorage(int width, int height);
	bool rebuildUIPanelTextureLocked(const std::string& panelName, UIPanelTextureData& textureData, UIPanelFrameData& outFrame);
	bool translateMousePosition(float inputX, float inputY, int& outputX, int& outputY);
	void updateWindowProperties();
	void updateTexture(const void* buffer, int width, int height);
	void flushRuntimeLayoutSync();
	std::string buildRuntimeLayoutSyncScript(const std::map<std::string, UIPanelFrameData>& panelFrames) const;
	
	// SDL to CEF event conversion helpers
	uint32_t GetCefModifiers(const SDL_Event& event);
	uint32_t GetCefKeyboardModifiers(const SDL_Event& event);
	int GetWindowsKeyCode(SDL_Scancode scancode, SDL_Keycode key);
	
	IMPLEMENT_REFCOUNTING(CEF_Drawer);
};
