#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include "UIPanel.hpp"
#include "PanelMapBuilder.hpp"
#include "Splitter.hpp"
#include "SplitterMouseMecanic.hpp"
#include "FrameDatas/IFrameDatas.hpp"
#include "include/cef_browser.h"
#include <map>
#include <string>
#include <memory>
#include <mutex>

class VKContext;
class VulkanPipeline;
struct IFrameData;
enum WindowRenderState : int;

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
			void CreateNewMapFromSwitchCall(const std::map<std::string, IFrameData>& panels);
			void cacheUIPanelFrameDatas(SIMILI::Frontend::IFrameDatas* frameDatas, const std::map<std::string, IFrameScreenData>& splitterFrameDataMap);
			void syncFrameDatas(SIMILI::Frontend::IFrameDatas* frameDatas, SDL_Window* sdlWindow);

			void prepareUiPanelsForFullscreen(int drawableWidth, int drawableHeight, const std::map<std::string, IFrameScreenData>& panelFrameDataMap, SDL_Window* window, int referenceWindowWidth, int referenceWindowHeight);			
			void renderFullScreenUIPanelsInsideBorders(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, bool skipTextureRebuild, SDL_Window* window);
			
			void cleanUpDatasBeforeDrawingForReducedScreen();
			void drawReduceScreenUIpanelsInsideBorders(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, const std::map<std::string, IFrameScreenData>& panelFrameDataMap, bool skipTextureRebuild, SDL_Window* window, int referenceWindowWidth, int referenceWindowHeight);
			
			void drawSplitters(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight);
			void drawSplittersFullScreen(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, SDL_Window* window, int referenceWindowWidth, int referenceWindowHeight);
			void drawSplittersReducedScreenSize(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, SDL_Window* window, int referenceWindowWidth, int referenceWindowHeight);
			
			void enableSplitterMouseInteractions(int mouseX, int mouseY, bool isLeftButtonDown = false);
			void dragSplitter(int mouseX, int mouseY, bool isLeftButtonDown);
			
			void bindWorkSpaceSizeToSplitterInteractions();
			void bindPanelsToSplitters(WindowRenderState currentWindowState);
			void bindFullScreenPanelsToSplitters(WindowRenderState currentWindowState);
			
			void setBorders(int borderLeft, int borderTop, int borderWidth, int borderHeight);

			void FreezeCoordinatesForFullscreen(int fullscreenWidth, int fullscreenHeight);
			void ResetFullscreenFreeze();
			void FreezeModifiedPanelAtFullScreen();
			void ScaleDownPanels(int initWidth, int initHeight);
			
			void setCEFTextureForAllPanels(VkImageView view, VkSampler sampler, int cefWidth, int cefHeight);
			bool consumePendingCEFRepaintRequest();
			void requestPendingCEFRepaint();
			void refreshPanelTextureLayout(bool requestCEFRepaint = false);
		
			void clearUIPanels();
			void Clear_Everything();

			void forwardMouseEventsToPanels(int mouseX, int mouseY, bool isLeftButtonDown, bool isRightButtonDown, CefRefPtr<CefBrowser> browser);

			std::map<std::string, IFrameData> getUIPanelIFrames() const;
			std::map<std::string, IFrameData> getResolvedUIPanelIFrames() const;
			std::map<std::string, IFrameScreenData> getUIPanelFrameDatas() const;
			const WorkSpace& getWorkSpace() const;
			void setWorkSpace(int x, int y, int width, int height);
		
		private:
			void applyPanelTextureLayout(bool requestCEFRepaint = false);
			void applyFullScreenPanelTextureLayout(bool requestCEFRepaint, int fullscreenWidth, int fullscreenHeight);

			std::vector<SplitterDefinition> scaleSplittersToWindow(int targetWindowWidth, int targetWindowHeight, int referenceWindowWidth, int referenceWindowHeight) const;

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
			std::map<std::string, IFrameScreenData> ui_panel_geometry_frame_data_map_;
			std::map<std::string, std::unique_ptr<UIPanel>> ui_panels_;
			bool ui_panels_initialized_;
			mutable std::mutex ui_panel_mutex_;
			
			std::map<std::string, PanelState> panel_state_map_;
			std::vector<SplitterDefinition> splitter_list_;
			std::vector<SplitterDefinition> prev_splitter_list_;
			int last_window_width_ = 0;
			int last_window_height_ = 0;
			bool panel_frames_overridden_by_splitters_ = false;
			bool was_splitter_operating_ = false;
			bool pending_cef_repaint_request_ = false;
			bool pending_geometry_texture_layout_ = false;
			bool coordinates_frozen_for_fullscreen_ = false;
			bool had_init_splitter_panel_modification_before_maximized_ = false;
			int frozen_fullscreen_width_ = 0;
			int frozen_fullscreen_height_ = 0;
			WindowRenderState current_window_render_state_;

			PanelMapBuilder panel_map_builder_;
			std::unique_ptr<Splitter> splitter_renderer_;
			SplitterMouseMecanic splitter_mouse_mecanic_;

			VkImageView stored_cef_view_ = VK_NULL_HANDLE;
			VkSampler stored_cef_sampler_ = VK_NULL_HANDLE;
			int stored_cef_width_ = 0;
			int stored_cef_height_ = 0;
			// Actual dimensions of the CEF Vulkan texture currently uploaded to GPU.
			// Distinct from stored_cef_width_/height_ which may be frozen to the fullscreen logical size.
			// Used for correct UV coordinate calculation in applyFullScreenPanelTextureLayout.
			int actual_cef_texture_width_ = 0;
			int actual_cef_texture_height_ = 0;
			std::map<std::string, std::pair<float, float>> last_fullscreen_uv_size_map_;
			std::map<std::string, IFrameScreenData> frozen_modified_fullscreen_map_;
			bool has_frozen_modified_fullscreen_map_ = false;
			std::map<std::string, IFrameScreenData> fullscreen_prepared_frame_data_map_;
			std::map<std::string, IFrameScreenData> reduced_prepared_frame_data_map_;
			std::map<int, std::vector<std::string>> pending_fullscreen_modified_panels_by_splitter_;
			std::map<int, std::vector<std::string>> pending_fullscreen_tracked_panels_by_splitter_;
			std::map<int, std::map<std::string, IFrameScreenData>> pending_fullscreen_panel_frames_by_splitter_;
			SDL_Window* stored_window_ = nullptr;
		};
	}
}
