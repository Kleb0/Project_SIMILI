#pragma once

#include "../FrameDatas/FrameDatas.hpp"
#include "../../../Engine/VulkanPipeline/VulkanPipeline.hpp"
#include "PanelMapBuilder.hpp"
#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <memory>

class VKContext;

class Splitter
{
public:
	Splitter();
	~Splitter();

	bool initialize(SDL_Window* window, VKContext* vkContext, VkRenderPass renderPass);
	bool finalizeInitialization();
	void shutdown();
	void syncFrameDatas(const SIMILI::Frontend::FrameDatas* frameDatas);
	bool handleEvent(const SDL_Event& event);
	void draw(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight);

	bool getViewportFrameData(SIMILI::Frontend::IFrameScreenData& outData) const;
	std::map<std::string, SIMILI::Frontend::IFrameScreenData> getUIPanelFrameDatas() const;
	std::map<std::string, SIMILI::Frontend::IFrameScreenData> getAllFrameDatas() const;
	bool isReady() const;
	bool isDragging() const { return dragging_; }
	void setVulkanPipelines(VulkanPipeline* pipelines);
	void forceRefreshLayout();

private:
	enum class Axis
	{
		Vertical,
		Horizontal
	};

	enum class Type
	{
		HierarchyViewport,
		ViewportInspector,
		InspectorHistory,
		TopProjectViewer
	};

	struct SplitterGeometry
	{
		Type type;
		Axis axis;
		int x;
		int y;
		int width;
		int height;
	};

	SDL_Window* window_;
	VKContext* vk_context_;
	VulkanPipeline* vulkan_pipelines_;
	VkRenderPass vk_render_pass_;
	bool initialized_;
	bool layout_ready_;
	bool dragging_;
	int active_splitter_index_;
	int hovered_splitter_index_;
	int drag_anchor_;
	int last_window_width_;
	int last_window_height_;
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
	std::map<std::string, SIMILI::Frontend::PanelState> panel_state_map_;
	std::map<std::string, SIMILI::Frontend::IFrameScreenData> source_frame_data_map_;
	std::vector<SplitterGeometry> splitters_;
	mutable std::mutex splitter_mutex_;
	SIMILI::Frontend::PanelMapBuilder panel_map_builder_;

	bool createGraphicsResources();
	void destroyGraphicsResources();
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	bool hasSourceGeometryChanged(const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& frameDataMap) const;
	bool shouldRefreshFromSource(const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& frameDataMap) const;
	void rebuildFromSource(const std::map<std::string, SIMILI::Frontend::IFrameScreenData>& frameDataMap, int currentWindowWidth, int currentWindowHeight);
	void scaleLayoutToWindow(int newWindowWidth, int newWindowHeight);
	void refreshDerivedData();
	void rebuildSplitters();
	bool resolveMousePosition(const SDL_Event& event, int& x, int& y) const;
	int pickSplitterIndex(int x, int y) const;
	void beginDrag(int splitterIndex, int x, int y);
	void endDrag();
	int applyDelta(const SplitterGeometry& splitter, int delta);
	int applyVerticalDelta(const std::string& leftPanelName, const std::string& rightPanelName, int delta, const std::vector<std::string>& linkedRightPanels);
	int applyHorizontalDelta(const std::string& topPanelName, const std::string& bottomPanelName, int delta);
	int applyTopRowDelta(int delta);
	void drawGeometry(VkCommandBuffer commandBuffer, const SplitterGeometry& splitter, int drawableWidth, int drawableHeight, float red, float green, float blue, float alpha);
};
