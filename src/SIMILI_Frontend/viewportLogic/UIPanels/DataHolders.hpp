#pragma once

#include "../../../Engine/VulkanPipeline/VulkanPipeline.hpp"
#include "../../../WorldObjects/Entities/ThreeDObject.hpp"
#include <string>
#include <vector>
#include <array>
#include <map>
#include <mutex>
#include <memory>
#include <vulkan/vulkan.h>

class VKContext;
class VKScene;

struct TextOverlay
{
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory  memory = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	std::string lastText;
	int lastWidth = 0;
	int lastHeight = 0;
};

struct FieldRect
{
	int x = 0, y = 0, w = 0, h = 0;
};

class DataHolders
{
public:
	DataHolders();
	~DataHolders() = default;

	static DataHolders* getInstance();
	static void setInstance(DataHolders* instance);

	void computeNewDataToDataHolders();
	int getCountForPanel(const std::string& panelName) const;

	void setFieldValue(const std::string& panelName, const std::string& field, const std::string& value);
	std::string getFieldValue(const std::string& panelName, const std::string& field) const;
	std::string getValuesAsJson(const std::string& panelName) const;

	void setFieldRect(const std::string& panelName, const std::string& field, int x, int y, int w, int h);

	void initPlaceholderValues();

	void drawTextData(
		const std::string& panelName,
		int panelDrawX, int panelDrawY, int panelDrawW, int panelDrawH,
		int logicalW, int logicalH,
		VkCommandBuffer cmdBuffer, int drawableW, int drawableH,
		VKContext* vkContext, VulkanPipeline* vkPipelines, VkRenderPass renderPass);

	void cleanupVulkan();

	std::shared_ptr<VKScene> getVKScene() const;
	void setVKScene(std::shared_ptr<VKScene> scene);
	std::shared_ptr<VKScene> vk_scene_;

	std::map<std::string, std::vector<std::string>> data_field_names_; 
	std::map<std::string, std::map<std::string, std::string>> data_holder_values_; 


	// turn dataHolder_values as list 

	// DataHolderEntry definition

	void setSelectedObjectsForDataHolders(const std::list<ThreeDObject*>& objects);
	std::list<ThreeDObject*> getSelectedObjectsForDataHolders() const { return selectedObjectsForDataHolders_; }



private:

	std::list<ThreeDObject*> selectedObjectsForDataHolders_;
	static DataHolders* s_instance_;	

	bool initializeVulkan(VKContext* context, VulkanPipeline* pipelines, VkRenderPass renderPass);
	bool createVertexBuffer();
	bool uploadTextTexture(const std::string& text, int width, int height, TextOverlay& overlay);
	void cleanupTextOverlay(TextOverlay& overlay);
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

	static std::vector<uint8_t> rasterizeText(const std::string& text, int width, int height);

	mutable std::mutex mutex_;

	// Deprecated: using DataHolderEntry
	// std::map<std::string, std::vector<DataHolderEntry>> panel_fields_;

	std::map<std::string, std::pair<float,float>> panel_viewport_fracs_;
	std::map<std::string, std::map<std::string, std::string>> field_values_;
	std::map<std::string, std::map<std::string, FieldRect>> field_rects_;

	VKContext* vk_context_ = nullptr;
	VulkanPipeline* vulkan_pipelines_ = nullptr;
	VkRenderPass vk_render_pass_ = VK_NULL_HANDLE;

	std::shared_ptr<VulkanPipeline::Pipeline> text_pipeline_;
	VkDescriptorSetLayout descriptor_set_layout_ = VK_NULL_HANDLE;
	VkDescriptorPool descriptor_pool_  = VK_NULL_HANDLE;

	VkBuffer vertex_buffer_ = VK_NULL_HANDLE;
	VkDeviceMemory  vertex_buffer_memory_ = VK_NULL_HANDLE;
	bool vulkan_initialized_ = false;

	static constexpr int kMaxOverlays = 64;
	std::array<TextOverlay, kMaxOverlays> text_overlays_;
};
