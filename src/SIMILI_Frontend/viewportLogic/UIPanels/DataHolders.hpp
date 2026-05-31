#pragma once

#include "../../../Engine/VulkanPipeline/VulkanPipeline.hpp"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <memory>
#include <vulkan/vulkan.h>

class VKContext;

struct DataHolderEntry
{
	std::string field;
	int relX;
	int relY;
	int width;
	int height;
};

class DataHolders
{
public:
	DataHolders();
	~DataHolders() = default;

	static DataHolders* getInstance();
	static void setInstance(DataHolders* instance);

	bool receiveDataHolders(const std::string& jsonBody);
	int getCountForPanel(const std::string& panelName) const;

	void drawForPanel(
		const std::string& panelName,
		int panelDrawX, int panelDrawY, int panelDrawW, int panelDrawH,
		int logicalW, int logicalH,
		VkCommandBuffer cmdBuffer, int drawableW, int drawableH,
		VKContext* vkContext, VulkanPipeline* vkPipelines, VkRenderPass renderPass);

	void cleanupVulkan();

private:
	static DataHolders* s_instance_;

	bool initializeVulkan(VKContext* context, VulkanPipeline* pipelines, VkRenderPass renderPass);
	bool createVertexBuffer();
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	mutable std::mutex mutex_;
	std::map<std::string, std::vector<DataHolderEntry>> panel_fields_;
	std::map<std::string, std::pair<float,float>> panel_viewport_fracs_;  

	VKContext* vk_context_ = nullptr;
	VulkanPipeline* vulkan_pipelines_ = nullptr;
	VkRenderPass vk_render_pass_ = VK_NULL_HANDLE;
	std::shared_ptr<VulkanPipeline::Pipeline> overlay_pipeline_;
	VkBuffer vertex_buffer_ = VK_NULL_HANDLE;
	VkDeviceMemory vertex_buffer_memory_  = VK_NULL_HANDLE;
	bool vulkan_initialized_ = false;

	static constexpr int kMaxOverlays = 64;
};
