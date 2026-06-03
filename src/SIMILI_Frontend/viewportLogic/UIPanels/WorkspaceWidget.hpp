#pragma once

#include "include/cef_browser.h"
#include "include/cef_client.h"
#include "include/cef_render_handler.h"
#include "../../../Engine/VulkanPipeline/VulkanPipeline.hpp"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>

class VKContext;

namespace SIMILI {
namespace Frontend {

class WorkspaceWidget;

class WidgetRenderHandler : public CefRenderHandler
{
public:
	explicit WidgetRenderHandler(WorkspaceWidget* owner);
	void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
	void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
	             const RectList& dirtyRects, const void* buffer, int width, int height) override;

private:
	WorkspaceWidget* owner_;
	IMPLEMENT_REFCOUNTING(WidgetRenderHandler);
};

class WidgetCefClient : public CefClient
{
public:
	explicit WidgetCefClient(WorkspaceWidget* owner);
	CefRefPtr<CefRenderHandler> GetRenderHandler() override;

private:
	WorkspaceWidget* owner_;
	CefRefPtr<WidgetRenderHandler> render_handler_;
	IMPLEMENT_REFCOUNTING(WidgetCefClient);
};

class WorkspaceWidget
{
public:
	WorkspaceWidget();
	~WorkspaceWidget();

	bool initializeVulkan(VKContext* vkContext, VulkanPipeline* pipelines,
	                      VkRenderPass renderPass, int widgetWidth, int widgetHeight);
	void loadURL(const std::string& url);
	void shutdown();

	void onPaint(const void* buffer, int width, int height);
	void uploadPaintBuffer();
	void filterColor(std::vector<unsigned char>& pixelData, int width, int height,
	                 unsigned char r, unsigned char g, unsigned char b, unsigned char threshold);
	void draw(VkCommandBuffer commandBuffer, int wsX, int wsY, int drawableWidth, int drawableHeight);
	void sendKeyEvent(const std::string& key);

	int getWidgetWidth()  const { return widget_width_;  }
	int getWidgetHeight() const { return widget_height_; }

private:
	bool createTexture();
	bool createVertexBuffer();
	bool createDescriptorSet();
	bool createPipeline();
	void cleanupVulkanResources();
	void updateGeometry(int wsX, int wsY, int drawableWidth, int drawableHeight);
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	VKContext* vk_context_;
	VulkanPipeline* vulkan_pipelines_;
	VkRenderPass vk_render_pass_;

	VkImage vk_texture_image_;
	VkDeviceMemory vk_texture_memory_;
	VkImageView vk_texture_view_;
	VkSampler vk_sampler_;

	VkDescriptorPool vk_descriptor_pool_;
	VkDescriptorSetLayout vk_descriptor_set_layout_;
	VkDescriptorSet vk_descriptor_set_;

	VkBuffer vk_vertex_buffer_;
	VkDeviceMemory vk_vertex_buffer_memory_;

	std::shared_ptr<VulkanPipeline::Pipeline> shared_pipeline_;

	CefRefPtr<WidgetCefClient> cef_client_;
	CefRefPtr<CefBrowser> cef_browser_;

	mutable std::mutex paint_mutex_;
	std::vector<unsigned char> paint_buffer_;
	int paint_width_;
	int paint_height_;

	int  texture_uploaded_width_;
	int texture_uploaded_height_;
	VkImageView bound_texture_view_;
	VkSampler bound_sampler_;

	int  widget_width_;
	int  widget_height_;
	bool vulkan_initialized_;
	bool has_texture_;
	int last_ws_x_;
	int last_ws_y_;
	bool geometry_dirty_;
};

} // namespace Frontend
} // namespace SIMILI
