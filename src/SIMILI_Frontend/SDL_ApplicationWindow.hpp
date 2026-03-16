#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <iostream>

/**
 * @brief Manages the main SDL application window
 * 
 * This class encapsulates the SDL window creation, positioning, and state management.
 * It serves as the main container for both CEF UI rendering and 3D viewport overlay.
 */
class SDL_ApplicationWindow
{
public:
	SDL_ApplicationWindow();
	~SDL_ApplicationWindow();
	
	// Window lifecycle
	bool create(const std::string& title, int width, int height, Uint32 flags = 0);
	void destroy();
	
	// Window properties
	void setPosition(int x, int y);
	void setSize(int width, int height);
	void setTitle(const std::string& title);
	
	// Window state
	void show();
	void hide();
	void maximize();
	void restore();
	bool isMaximized() const;
	bool isVisible() const;
	
	// Position and size queries
	void getPosition(int& x, int& y) const;
	void getSize(int& width, int& height) const;
	SDL_Rect getBounds() const;
	
	// Border offsets (for maximized state on Windows)
	void getBorderOffsets(int& left, int& top, int& right, int& bottom) const;
	
	// DPI handling
	float getDpiScale() const;
	
	// Window handle access
	SDL_Window* getHandle() const { return window_; }
	bool isValid() const { return window_ != nullptr; }
	void Set_UIHandler(void* handler);
		
	// Event handling helper
	void processEvents();
	
private:
	SDL_Window* window_;
	bool is_maximized_;
	int last_x_;
	int last_y_;
	int last_width_;
	int last_height_;
	float dpi_scale_;
	void* ui_handler_;
	
	void updateDpiScale();
	void updateMaximizedState();
	void captureFrameData();
};
