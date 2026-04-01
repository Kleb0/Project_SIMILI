#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <mutex>

namespace SIMILI
{
	namespace Frontend
	{
		class FrameDatas;
	}
}

class Camera;
class VKContext;

class ThreeDScreen
{
public:
	ThreeDScreen();
	~ThreeDScreen();
	
	void initialize();
	void render(SIMILI::Frontend::FrameDatas* frameDatas, SDL_Window* window);
	void draw(VkCommandBuffer commandBuffer, VkRenderPass renderPass, VkFramebuffer framebuffer);
	void setCamera(Camera* camera) { camera_ = camera; }
	Camera* getCamera() const { return camera_; }
	void setVKContext(VKContext* context);
	
	int getX() const { return x_; }
	int getY() const { return y_; }
	int getWidth() const { return width_; }
	int getHeight() const { return height_; }
	bool hasValidViewport() const { return has_valid_viewport_; }
	
private:
	int x_;
	int y_;
	int width_;
	int height_;
	bool initialized_;
	bool has_valid_viewport_;
	bool waiting_for_frame_data_logged_;
	bool first_render_logged_;
	Camera* camera_;
	VKContext* vk_context_;
	std::mutex render_mutex_;
};
