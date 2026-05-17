#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <string>
#include "../../../Engine/VulkanPipeline/VulkanPipeline.hpp"

class SplitterMouseMecanic;

class VKContext;

class Splitter
{
public:
	struct SplitterData
	{
		int x;
		int y;
		int width;
		int height;
		bool isVertical;
		bool isHorizontal;
		std::string id;
	};

	Splitter();
	~Splitter();

	bool initialize(VKContext* vkContext, VkRenderPass renderPass);
	void shutdown();
	void draw(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight);

	void setSplitters(const std::vector<SplitterData>& splitters);
	void setVulkanPipelines(VulkanPipeline* pipelines);
	void setHoveredIndex(int index);
	void setSplitterMouseMecanic(SplitterMouseMecanic* mecanic);

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
	std::vector<SplitterData> splitters_;
	int hovered_index_;
	SplitterMouseMecanic* splitter_mouse_mecanic_;

	bool createGraphicsResources();
	void destroyGraphicsResources();
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	void drawSplitter(VkCommandBuffer commandBuffer, const SplitterData& splitter, int drawableWidth, int drawableHeight);
};
