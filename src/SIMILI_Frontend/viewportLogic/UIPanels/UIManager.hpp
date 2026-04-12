#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include "UIPanel.hpp"
#include "FrameDatas/FrameDatas.hpp"
#include <map>
#include <string>
#include <memory>
#include <mutex>

class VKContext;
class VulkanPipeline;
class CEF_Drawer;
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
			void setCEFDrawer(CEF_Drawer* drawer) { cef_drawer_ = drawer; }

			void initializeDefaultUIPanels();
			void updateUIPanelIFrames(const std::map<std::string, IFrameData>& iframeDataMap);
			void cacheUIPanelFrameDatas(SIMILI::Frontend::FrameDatas* frameDatas, const std::map<std::string, IFrameScreenData>& splitterFrameDataMap);
			void drawUIPanels(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, const std::map<std::string, IFrameScreenData>& panelFrameDataMap, bool skipTextureRebuild);
			void clearUIPanels();
			void forceRebuildAllUIPanels();
			void forceRedrawAllUIPanels();

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
			CEF_Drawer* cef_drawer_;

			std::map<std::string, IFrameData> ui_panel_iframe_map_;
			std::map<std::string, IFrameScreenData> ui_panel_frame_data_map_;
			std::map<std::string, std::unique_ptr<UIPanel>> ui_panels_;
			bool ui_panels_initialized_;
			mutable std::mutex ui_panel_mutex_;
		};
	}
}
