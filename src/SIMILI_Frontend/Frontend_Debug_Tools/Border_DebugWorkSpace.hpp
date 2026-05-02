#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include "../../Engine/VulkanPipeline/VulkanPipeline.hpp"

class VKContext;

namespace SIMILI {
	namespace Frontend {
		class WorkSpace;
	}
}

class Border_DebugWorkSpace
{
public:
	Border_DebugWorkSpace();
	~Border_DebugWorkSpace();

	bool initialize(VKContext* vkContext, VkRenderPass renderPass);
	void shutdown();
	void draw(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, const SIMILI::Frontend::WorkSpace* workspace);

	void setVulkanPipelines(VulkanPipeline* pipelines);

private:
	VKContext* vk_context_;
	VulkanPipeline* vulkan_pipelines_;
	VkRenderPass vk_render_pass_;
	bool initialized_;
	VkBuffer vk_vertex_buffer_;
	VkDeviceMemory vk_vertex_buffer_memory_;
	VkDescriptorSetLayout vk_descriptor_set_layout_;
	VkDescriptorPool vk_descriptor_pool_;
	VkDescriptorSet vk_descriptor_set_;
	VkImage dummy_texture_image_;
	VkDeviceMemory dummy_texture_memory_;
	VkImageView dummy_texture_view_;
	VkSampler dummy_texture_sampler_;
	std::shared_ptr<VulkanPipeline::Pipeline> shared_pipeline_;

	bool createGraphicsResources();
	void destroyGraphicsResources();
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
};
