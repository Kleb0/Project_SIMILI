#pragma once

#include <vulkan/vulkan.h>

class VKContext;
class VulkanPipeline;
class App_Border;
class Border_DebugRectangle;

class Enable_UI_Debug_Tools
{
public:
	Enable_UI_Debug_Tools();
	~Enable_UI_Debug_Tools();

	void initialize(VKContext* vkContext, VkRenderPass renderPass, VulkanPipeline* pipelines, App_Border* appBorder);
	void shutdown();

	void activateDebugRender();
	void drawDebugTools(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight);

	void setDebugEnabled(bool enabled);
	bool isDebugEnabled() const;

private:
	VKContext* vk_context_;
	VkRenderPass vk_render_pass_;
	VulkanPipeline* vulkan_pipelines_;
	App_Border* app_border_;
	Border_DebugRectangle* border_debug_rectangle_;
	bool debug_enabled_;
	bool initialized_;
};
