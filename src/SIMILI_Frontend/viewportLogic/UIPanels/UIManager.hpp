#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include "UIPanel.hpp"
#include "PanelMapBuilder.hpp"
#include "Splitter.hpp"
#include "FrameDatas/FrameDatas.hpp"
#include <map>
#include <string>
#include <memory>
#include <mutex>

class VKContext;
class VulkanPipeline;
struct IFrameData;

namespace SIMILI {
	namespace Frontend {

		enum class UIState
		{
			Normal,
			Maximized,
			Minimized,
			Resizing,
			Moving
		};

		class UIManager
		{
		public:
			UIManager();
			~UIManager();

			void updateUIState(SDL_Window* window);

			UIState getCurrentState() const { return current_state_; }
			bool hasStateChanged() const { return state_changed_; }
			void acknowledgeStateChange() { state_changed_ = false; }

			int getWindowWidth() const { return window_width_; }
			int getWindowHeight() const { return window_height_; }
			int getWindowX() const { return window_x_; }
			int getWindowY() const { return window_y_; }
			bool isWindowMaximized() const { return is_maximized_; }

			void setVKContext(VKContext* context) { vk_context_ = context; }
			void setVulkanPipelines(VulkanPipeline* pipelines) { vulkan_pipelines_ = pipelines; }
			void setRenderPass(VkRenderPass renderPass) { vk_render_pass_ = renderPass; }

			void initializeDefaultUIPanels();
			void updateUIPanelIFrames(const std::map<std::string, IFrameData>& iframeDataMap);
			void cacheUIPanelFrameDatas(SIMILI::Frontend::FrameDatas* frameDatas, const std::map<std::string, IFrameScreenData>& splitterFrameDataMap);
			void syncFrameDatas(SIMILI::Frontend::FrameDatas* frameDatas, SDL_Window* sdlWindow);
			
			void drawUIPanelsInsideBorders(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, const std::map<std::string, IFrameScreenData>& panelFrameDataMap, bool skipTextureRebuild, SDL_Window* window);
			void drawFullScreenUIPanelsInsideBorders(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, const std::map<std::string, IFrameScreenData>& panelFrameDataMap, bool skipTextureRebuild, SDL_Window* window, int referenceWindowWidth, int referenceWindowHeight);
			void drawSplitters(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight);
			void setBorders(int borderLeft, int borderTop, int borderWidth, int borderHeight);
			void setCEFTextureForAllPanels(VkImageView view, VkSampler sampler, int cefWidth, int cefHeight);
		
			void clearUIPanels();

			std::map<std::string, IFrameData> getUIPanelIFrames() const;
			std::map<std::string, IFrameScreenData> getUIPanelFrameDatas() const;
		
		private:
			UIState current_state_;
			UIState previous_state_;
			bool state_changed_;

			int window_width_;
			int window_height_;
			int window_x_;
			int window_y_;
			bool is_maximized_;

			int previous_width_;
			int previous_height_;
			int previous_x_;
			int previous_y_;
			bool previous_maximized_;

			VKContext* vk_context_;
			VulkanPipeline* vulkan_pipelines_;
			VkRenderPass vk_render_pass_;

			int app_border_left_;
			int app_border_top_;
			int app_border_width_;
			int app_border_height_;

			std::map<std::string, IFrameData> ui_panel_iframe_map_;
			std::map<std::string, IFrameScreenData> ui_panel_frame_data_map_;
			std::map<std::string, std::unique_ptr<UIPanel>> ui_panels_;
			bool ui_panels_initialized_;
			mutable std::mutex ui_panel_mutex_;
			
			std::map<std::string, PanelState> panel_state_map_;
			std::vector<SplitterDefinition> splitter_list_;
			int last_window_width_  = 0;
			int last_window_height_ = 0;

			PanelMapBuilder panel_map_builder_;
			std::unique_ptr<Splitter> splitter_renderer_;
		};
	}
}
