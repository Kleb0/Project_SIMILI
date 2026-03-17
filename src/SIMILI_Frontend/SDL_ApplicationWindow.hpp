#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <iostream>
#include <map>

class ThreeDScreen;

struct IFrameData;

namespace SIMILI
{
	namespace Frontend
	{
		class FrameDatas;
	}
}

class SDL_ApplicationWindow
{
public:
	SDL_ApplicationWindow();
	~SDL_ApplicationWindow();
	
	bool create(const std::string& title, int width, int height, Uint32 flags = 0);
	void destroy();
	
	void setPosition(int x, int y);
	void setSize(int width, int height);
	void setTitle(const std::string& title);
	
	void show();
	void hide();
	void maximize();
	void restore();
	bool isMaximized() const;
	bool isVisible() const;
	
	void getPosition(int& x, int& y) const;
	void getSize(int& width, int& height) const;
	SDL_Rect getBounds() const;
	
	void getBorderOffsets(int& left, int& top, int& right, int& bottom) const;
	
	float getDpiScale() const;
	
	SDL_Window* getHandle() const { return window_; }
	bool isValid() const { return window_ != nullptr; }
	void Set_UIHandler(void* handler);
	
	void setThreeDScreen(ThreeDScreen* screen);
	void renderThreeDScreen(const std::map<std::string, IFrameData>& frameDataMap);
	void drawThreeDScreen();
	void updateFrameDatas(SIMILI::Frontend::FrameDatas* frameDatas);
		
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
	ThreeDScreen* threed_screen_;
	SIMILI::Frontend::FrameDatas* frame_datas_;
	
	void updateDpiScale();
	void updateMaximizedState();
	void captureFrameData();
};
