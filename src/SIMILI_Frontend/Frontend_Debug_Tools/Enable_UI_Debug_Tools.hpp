#pragma once

#include <vulkan/vulkan.h>
#include <map>
#include <string>

class VKContext;
class VulkanPipeline;
class App_Border;
class Border_DebugRectangle;
class Border_DebugWorkSpace;

namespace SIMILI {
	namespace Frontend {
		struct IFrameScreenData;
		class WorkSpace;
	}
}

class Enable_UI_Debug_Tools
{
public:
	Enable_UI_Debug_Tools();
	~Enable_UI_Debug_Tools();

	void initialize(VKContext* vkContext, VkRenderPass renderPass, VulkanPipeline* pipelines, App_Border* appBorder);
	void shutdown();

	void activateDebugRender();
	void drawDebugTools(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, const SIMILI::Frontend::WorkSpace* workspace);

	void setDebugEnabled(bool enabled);
	bool isDebugEnabled() const;

private:
	VKContext* vk_context_;
	VkRenderPass vk_render_pass_;
	VulkanPipeline* vulkan_pipelines_;
	App_Border* app_border_;
	Border_DebugRectangle* border_debug_rectangle_;
	Border_DebugWorkSpace* border_debug_workspace_;
	bool debug_enabled_;
	bool initialized_;
};
