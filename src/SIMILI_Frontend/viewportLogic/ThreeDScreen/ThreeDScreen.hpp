#pragma once

#include <SDL3/SDL.h>
#include <glad/glad.h>
#include <mutex>

namespace SIMILI
{
	namespace Frontend
	{
		class FrameDatas;
	}
}

class ThreeDScreen
{
public:
	ThreeDScreen();
	~ThreeDScreen();
	
	void initialize();
	void render(SIMILI::Frontend::FrameDatas* frameDatas, SDL_Window* window, const SIMILI::Frontend::IFrameScreenData* viewportFrameData = nullptr);
	void draw();
	
private:
	int x_;
	int y_;
	int width_;
	int height_;
	bool initialized_;
	bool has_valid_viewport_;
	bool waiting_for_frame_data_logged_;
	bool first_render_logged_;
	std::mutex render_mutex_;
};
