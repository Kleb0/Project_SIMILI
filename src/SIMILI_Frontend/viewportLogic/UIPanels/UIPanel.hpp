#pragma once

#include "../FrameDatas/IFrameDatas.hpp"
#include "../../../Engine/VulkanPipeline/VulkanPipeline.hpp"
#include "include/cef_browser.h"
#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <string>
#include <memory>

class VKContext;
enum WindowRenderState : int;

class UIPanel
{
	public:

		enum class DrawingState
		{
			IsNotReadyToBeDrawn,
			IsReadyToBeDrawn,
			HasBeenDrawn
		};

		UIPanel();
		~UIPanel();

		bool initialize(const std::string& panelName);
		void shutdown();
		void PreventClippingForReducedandMaxizimizedWindows(WindowRenderState windowState);
		void draw(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, const SIMILI::Frontend::IFrameScreenData& frameData, SDL_Window* window, bool skipTextureRebuild, int appBorderLeft = 0, int appBorderTop = 0);
		void drawDataHolderOverlays(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, int logicalW, int logicalH);

		void setVKContext(VKContext* context);
		void setRenderPass(VkRenderPass renderPass);
		void setVulkanPipelines(VulkanPipeline* pipelines);
		void setCEFTexture(VkImageView view, VkSampler sampler, float u0, float v0, float u1, float v1);
		void refreshCEFTextureBinding(VkImageView view, VkSampler sampler);	
		void invalidateExternalTexture();		
		bool makePanelTextureInteractible(int mouseX, int mouseY, bool isLeftButtonDown, bool isRightButtonDown, CefRefPtr<CefBrowser> browser, int cefTextureWidth, int cefTextureHeight);
		const std::string& getName() const { return name_; }
		DrawingState getDrawingState() const { return drawing_state_; }

	private:
		bool createTexture();
		bool createShaderProgram();
		void updateGeometry(int drawableWidth, int drawableHeight);

		bool createVulkanResources();
		void cleanupVulkanResources();
		bool createVulkanPipeline();
		bool createVulkanVertexBuffer();
		bool createVulkanDescriptorSet();
		bool updateDescriptorTextureBinding();

		uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

		std::string name_;
		VkDescriptorPool vk_descriptor_pool_;
		VkDescriptorSetLayout vk_descriptor_set_layout_;
		float red_;
		float green_;
		float blue_;

		int horizontal_position_;
		int vertical_position_;
		int width_;
		int height_;

		float texcoord_left_;
		float texcoord_top_;
		float texcoord_right_;
		float texcoord_bottom_;

		bool initialized_;
		bool has_valid_bounds_;
		bool needs_redraw_;
		bool first_draw_done_;



		bool prevent_window_clipping_;
		bool prev_left_button_down_;
		bool prev_right_button_down_;
		bool prev_mouse_inside_;
		int prev_cef_mouse_x_;
		int prev_cef_mouse_y_;		

		DrawingState drawing_state_;
		VKContext* vk_context_;
		VulkanPipeline* vulkan_pipelines_;
		VkRenderPass vk_render_pass_;
		VkImage vk_texture_image_;
		VkDeviceMemory vk_texture_memory_;
		VkImageView vk_texture_view_;
		VkSampler vk_sampler_;
		VkDescriptorSet vk_descriptor_set_;
		std::shared_ptr<VulkanPipeline::Pipeline> shared_pipeline_;
		VkBuffer vk_vertex_buffer_;
		VkDeviceMemory vk_vertex_buffer_memory_;
		VkImageView external_texture_view_;
		VkSampler external_sampler_;
		VkImageView bound_texture_view_;
		VkSampler bound_sampler_;
};
