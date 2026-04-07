#pragma once

#include "include/cef_render_handler.h"
#include "include/cef_client.h"
#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <map>
#include <mutex>
#include <vector>
#include <iostream>
#include "../../Engine/VulkanPipeline/VulkanPipeline.hpp"

class CEF_Resizer;
class VKContext;


class CEF_Drawer : public CefRenderHandler
{
public:
	// Type alias for cleaner code
	using PipelinePtr = std::shared_ptr<VulkanPipeline::Pipeline>;

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
		VkImage texture_image = VK_NULL_HANDLE;
		VkDeviceMemory texture_memory = VK_NULL_HANDLE;
		VkImageView texture_view = VK_NULL_HANDLE;
		int width = 0;
		int height = 0;
		bool dirty = true;
		bool texture_layout_initialized = false;
	};

	CEF_Drawer();
	virtual ~CEF_Drawer();
	
	bool initialize(SDL_Window* window, VKContext* vkContext, VulkanPipeline* vulkanPipelines);
	void shutdown();
	void syncWindowProperties();
	void setRenderPass(VkRenderPass renderPass);
	bool hasPipeline() const { return shared_pipeline_ && shared_pipeline_->pipeline != VK_NULL_HANDLE; }
	
	// Create CEF browser with specified URL
	bool createBrowser(CefRefPtr<CefClient> client, const std::string& url, int width, int height);
	
	void draw(VkCommandBuffer commandBuffer);
	
	// Handle SDL events and forward them to CEF browser
	void handleEvent(const SDL_Event& event);
	
	// CefRenderHandler interface
	void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
	void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
	             const RectList& dirtyRects, const void* buffer,
	             int width, int height) override;
	
	// Resize handling
	void updateUIPanelFrames(const std::map<std::string, UIPanelFrameData>& panelFrames);
	void updateUIPanelDisplayFrame(const std::string& panelName, const UIPanelFrameData& panelFrame);
	void updateUIPanelSourceFrame(const std::string& panelName, const UIPanelFrameData& sourceFrame);
	bool getUIPanelTextureRegion(const std::string& panelName, VkImageView& outTextureView, VkSampler& outSampler, int& outTextureWidth, int& outTextureHeight, UIPanelFrameData& outFrame);
	bool isUIPanelTextureDirty(const std::string& panelName);
	static bool getActiveUIPanelTextureRegion(const std::string& panelName, VkImageView& outTextureView, VkSampler& outSampler, int& outTextureWidth, int& outTextureHeight, UIPanelFrameData& outFrame);
	static bool isActiveUIPanelTextureDirty(const std::string& panelName);
	static void updateActiveUIPanelDisplayFrame(const std::string& panelName, const UIPanelFrameData& panelFrame);
	static void updateActiveUIPanelSourceFrame(const std::string& panelName, const UIPanelFrameData& sourceFrame);
	static void requestActiveRuntimeLayoutSync(const std::map<std::string, UIPanelFrameData>& panelFrames);
	static void forceActiveLayoutSync();
	void requestRuntimeLayoutSync(const std::map<std::string, UIPanelFrameData>& panelFrames);
	void forceRepaint();
	void invalidateAllUIPanelTextures();
	CEF_Resizer& getResizer();
	
	// Accessors
	int getWidth() const { return width_; }
	int getHeight() const { return height_; }
	bool isInitialized() const { return initialized_; }
	SDL_Window* getWindowHandle() const { return window_; }
	SDLWindowProperties getSDLWindowProperties();
	
private:
	SDL_Window* window_;
	VKContext* vk_context_;
	VulkanPipeline* vulkan_pipelines_;
	PipelinePtr shared_pipeline_;  // Shared pipeline from VulkanPipeline factory
	VkRenderPass vk_render_pass_;
	VkImage texture_image_;
	VkDeviceMemory texture_memory_;
	VkImageView texture_view_;
	VkSampler texture_sampler_;
	VkBuffer vertex_buffer_;
	VkDeviceMemory vertex_buffer_memory_;
	VkShaderModule vertex_shader_;  // DEPRECATED: Now handled by VulkanPipeline
	VkShaderModule fragment_shader_;  // DEPRECATED: Now handled by VulkanPipeline
	VkPipeline pipeline_;  // DEPRECATED: Use shared_pipeline_->pipeline instead
	VkPipelineLayout pipeline_layout_;  // DEPRECATED: Use shared_pipeline_->layout instead
	VkDescriptorPool descriptor_pool_;
	VkDescriptorSetLayout descriptor_set_layout_;
	VkDescriptorSet descriptor_set_;
	
	int width_;
	int height_;
	int logical_width_;
	int logical_height_;
	int drawable_width_;
	int drawable_height_;
	float dpi_scale_;
	bool initialized_;
	bool texture_layout_initialized_;
	
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
	bool has_received_first_paint_;
	bool is_initialized_render_complete_;
	bool suppress_cef_repaints_;
	bool force_single_repaint_;
	int initial_paint_count_;
	bool descriptor_needs_update_;
	VkImageView last_bound_texture_view_;
	
	std::mutex render_mutex_;
	static CEF_Drawer* active_instance_;
	std::unique_ptr<CEF_Resizer> resizer_;
	
	bool createVulkanResources();
	bool createVulkanShaders();
	bool createVulkanPipeline();
	bool createVertexBuffer();
	void cleanupVulkanResources();
	void updateWindowProperties();
	void updateTexture(const void* buffer, int width, int height);
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	
	// SDL to CEF event conversion helpers
	uint32_t GetCefModifiers(const SDL_Event& event);
	uint32_t GetCefKeyboardModifiers(const SDL_Event& event);
	int GetWindowsKeyCode(SDL_Scancode scancode, SDL_Keycode key);

	friend class CEF_Resizer;
	
	IMPLEMENT_REFCOUNTING(CEF_Drawer);
};
