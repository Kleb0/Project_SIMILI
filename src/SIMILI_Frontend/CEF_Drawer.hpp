#pragma once

#include "include/cef_render_handler.h"
#include "include/cef_client.h"
#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <mutex>
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
	CEF_Drawer();
	virtual ~CEF_Drawer();
	
	// Initialize OpenGL resources for drawing CEF content
	bool initialize(SDL_Window* window);
	void shutdown();
	
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
	
	// Accessors
	int getWidth() const { return width_; }
	int getHeight() const { return height_; }
	bool isInitialized() const { return initialized_; }
	
private:
	SDL_Window* window_;
	GLuint texture_id_;
	GLuint vao_;
	GLuint vbo_;
	GLuint shader_program_;
	
	int width_;
	int height_;
	bool initialized_;
	
	CefRefPtr<CefBrowser> browser_;
	std::string url_;
	
	std::mutex render_mutex_;
	
	// OpenGL setup helpers
	bool createShaders();
	bool createQuad();
	void updateTexture(const void* buffer, int width, int height);
	
	// SDL to CEF event conversion helpers
	uint32_t GetCefModifiers(const SDL_Event& event);
	uint32_t GetCefKeyboardModifiers(const SDL_Event& event);
	int GetWindowsKeyCode(SDL_Scancode scancode, SDL_Keycode key);
	
	IMPLEMENT_REFCOUNTING(CEF_Drawer);
};
