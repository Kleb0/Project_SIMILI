#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <string>
#include <iostream>
#include <map>
#include <vector>

class ThreeDScreen;
class VKContext;
class VulkanPipeline;

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
	void setVKContext(VKContext* context);
	void setVulkanPipelines(VulkanPipeline* pipelines);
	VulkanPipeline* getVulkanPipelines() const { return vulkan_pipelines_; }
	void startSplitter();
	bool handleSplitterEvent(const SDL_Event& event);
	void renderThreeDScreen(const std::map<std::string, IFrameData>& frameDataMap);
	void drawThreeDScreen();
	void drawUIPanels();
	void drawCEF();
	void updateFrameDatas(SIMILI::Frontend::FrameDatas* frameDatas);
	void renderFrame();
	void setCurrentImageIndex(uint32_t index) { current_image_index_ = index; }
	
	VkSurfaceKHR getVulkanSurface() const { return vk_surface_; }
	VkSwapchainKHR getSwapchain() const { return vk_swapchain_; }
	const std::vector<VkImage>& getSwapchainImages() const { return vk_swapchain_images_; }
	const std::vector<VkImageView>& getSwapchainImageViews() const { return vk_swapchain_image_views_; }
	const std::vector<VkFramebuffer>& getFramebuffers() const { return vk_framebuffers_; }
	VkRenderPass getRenderPass() const { return vk_render_pass_; }
	VkCommandPool getCommandPool() const { return vk_command_pool_; }
	const std::vector<VkCommandBuffer>& getCommandBuffers() const { return vk_command_buffers_; }
	uint32_t getCurrentImageIndex() const { return current_image_index_; }
	VkSemaphore getImageAvailableSemaphore() const { return vk_image_available_semaphore_; }
	VkSemaphore getRenderFinishedSemaphore() const { return vk_render_finished_semaphore_; }
	VkFence getInFlightFence() const { return vk_in_flight_fence_; }
	bool initializeVulkan();
	void cleanupVulkan();
	bool recreateSwapchain();
		
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
	VKContext* vk_context_;
	VulkanPipeline* vulkan_pipelines_;
	
	VkSurfaceKHR vk_surface_;
	VkSwapchainKHR vk_swapchain_;
	std::vector<VkImage> vk_swapchain_images_;
	std::vector<VkImageView> vk_swapchain_image_views_;
	std::vector<VkFramebuffer> vk_framebuffers_;
	VkRenderPass vk_render_pass_;
	VkCommandPool vk_command_pool_;
	std::vector<VkCommandBuffer> vk_command_buffers_;
	uint32_t current_image_index_;
	uint32_t current_frame_;
	static const int MAX_FRAMES_IN_FLIGHT = 2;
	std::vector<VkSemaphore> vk_image_available_semaphores_;
	std::vector<VkSemaphore> vk_render_finished_semaphores_;
	std::vector<VkFence> vk_in_flight_fences_;
	VkSemaphore vk_image_available_semaphore_;
	VkSemaphore vk_render_finished_semaphore_;
	VkFence vk_in_flight_fence_;
	bool swapchain_needs_recreation_;
	
	void updateDpiScale();
	void updateMaximizedState();
	void captureFrameData();
	bool createSwapchain();
	bool createRenderPass();
	bool createFramebuffers();
	bool createCommandPool();
	bool createCommandBuffers();
	bool createSyncObjects();
};
