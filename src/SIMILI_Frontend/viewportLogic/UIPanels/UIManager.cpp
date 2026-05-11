#include "UIManager.hpp"
#include "SplitterMouseMecanic.hpp"
#include "../../SDL_ApplicationWindow.hpp"
#include "../../ThreadSafeIFrameMap.hpp"
#include "../../../Engine/VulkanScene/VKcontext.hpp"
#include <iostream>
#include <algorithm>
#include <set>
#include <cmath>

namespace SIMILI {
	namespace Frontend {

		namespace 
		{
			bool frameMapMatchesWindowSize(const std::map<std::string, IFrameScreenData>& frameDataMap, int windowWidth, int windowHeight)
			{
				for (const auto& pair : frameDataMap)
				{
					const IFrameScreenData& frame = pair.second;
					if (frame.windowWidth > 0 && frame.windowHeight > 0)
					{
						return frame.windowWidth == windowWidth && frame.windowHeight == windowHeight;
					}
				}

				return false;
			}
		}

		UIManager::UIManager()
			: current_state_(UIState::Normal)
			, previous_state_(UIState::Normal)
			, state_changed_(false)
			, window_width_(0)
			, window_height_(0)
			, window_x_(0)
			, window_y_(0)
			, is_maximized_(false)
			, previous_width_(0)
			, previous_height_(0)
			, previous_x_(0)
			, previous_y_(0)
			, previous_maximized_(false)
			, vk_context_(nullptr)
			, vulkan_pipelines_(nullptr)
			, vk_render_pass_(VK_NULL_HANDLE)
			, app_border_left_(0)
			, app_border_top_(0)
			, app_border_width_(0)
			, app_border_height_(0)
			, current_window_render_state_(WindowRenderState::Init)
			, ui_panels_initialized_(false)
			, coordinates_frozen_for_fullscreen_(false)
			, frozen_fullscreen_width_(0)
			, frozen_fullscreen_height_(0)
		{

		}

		UIManager::~UIManager()
		{
		}

		void UIManager::updateUIState(SDL_Window* window)
		{
			if (!window)
			{
				return;
			}

			previous_width_ = window_width_;
			previous_height_ = window_height_;
			previous_x_ = window_x_;
			previous_y_ = window_y_;
			previous_maximized_ = is_maximized_;
			previous_state_ = current_state_;

			SDL_GetWindowSize(window, &window_width_, &window_height_);
			SDL_GetWindowPosition(window, &window_x_, &window_y_);

			Uint32 flags = SDL_GetWindowFlags(window);
			is_maximized_ = (flags & SDL_WINDOW_MAXIMIZED) != 0;
			bool is_minimized = (flags & SDL_WINDOW_MINIMIZED) != 0;

			if (is_minimized)
			{
				current_state_ = UIState::Minimized;
			}
			else if (is_maximized_)
			{
				current_state_ = UIState::Maximized;
			}
			else if (previous_width_ != window_width_ || previous_height_ != window_height_)
			{
				current_state_ = UIState::Resizing;
			}
			else if (previous_x_ != window_x_ || previous_y_ != window_y_)
			{
				current_state_ = UIState::Moving;
			}
			else
			{
				current_state_ = UIState::Normal;
			}

			state_changed_ = (current_state_ != previous_state_);
        }

		void UIManager::initializeDefaultUIPanels()
		{
			std::lock_guard<std::mutex> lock(ui_panel_mutex_);
			ui_panels_.clear();
			ui_panels_initialized_ = false;
			std::cout << "[UIManager] Default UI panels initialized" << std::endl;
		}

		void UIManager::updateUIPanelIFrames(const std::map<std::string, IFrameData>& iframeDataMap)
		{
			std::lock_guard<std::mutex> lock(ui_panel_mutex_);
			ui_panel_iframe_map_.clear();

			for (const auto& pair : iframeDataMap)
			{
				if (pair.first == "viewport_panel")
				{
					continue;
				}

				ui_panel_iframe_map_[pair.first] = pair.second;
			}

			std::cout << "[UIManager] UI panel iframe count received: " << ui_panel_iframe_map_.size() << std::endl;
		}

		void UIManager::cacheUIPanelFrameDatas(SIMILI::Frontend::FrameDatas* frameDatas, const std::map<std::string, IFrameScreenData>& splitterFrameDataMap)
		{
			if (!splitterFrameDataMap.empty())
			{
				std::lock_guard<std::mutex> lock(ui_panel_mutex_);
				ui_panel_frame_data_map_ = splitterFrameDataMap;
				// Protect frozen geometry: do not overwrite with stale JS data while in fullscreen freeze.
				if (!panel_frames_overridden_by_splitters_ && !coordinates_frozen_for_fullscreen_)
				{
					ui_panel_geometry_frame_data_map_ = splitterFrameDataMap;
				}
				return;
			}

			const bool isNonInitState = (current_window_render_state_ == WindowRenderState::Maximized ||
			current_window_render_state_ == WindowRenderState::Reduced);

			if (!frameDatas)
			{
				return;
			}

			const auto& frameDataMap = frameDatas->getFrameData();
			std::lock_guard<std::mutex> lock(ui_panel_mutex_);
			std::map<std::string, IFrameScreenData> preservedGeometry = ui_panel_geometry_frame_data_map_;
			ui_panel_frame_data_map_.clear();
			// Protect frozen geometry: do not clear or overwrite when in fullscreen freeze.
			if (!panel_frames_overridden_by_splitters_ && !coordinates_frozen_for_fullscreen_)
			{
				ui_panel_geometry_frame_data_map_.clear();
			}

			if (ui_panel_iframe_map_.empty())
			{
				return;
			}

			for (const auto& pair : ui_panel_iframe_map_)
			{
				auto frameIt = frameDataMap.find(pair.first);
				if (frameIt != frameDataMap.end())
				{
					ui_panel_frame_data_map_[pair.first] = frameIt->second;
					if (panel_frames_overridden_by_splitters_ || coordinates_frozen_for_fullscreen_)
					{
						// Preserve existing geometry (scaled by splitters or frozen for fullscreen).
						auto preservedIt = preservedGeometry.find(pair.first);
						if (preservedIt != preservedGeometry.end())
						{
							ui_panel_geometry_frame_data_map_[pair.first] = preservedIt->second;
						}
						else
						{
							ui_panel_geometry_frame_data_map_[pair.first] = frameIt->second;
						}
					}
					else
					{
						ui_panel_geometry_frame_data_map_[pair.first] = frameIt->second;
					}
				}
			}
		}

		void UIManager::syncFrameDatas(SIMILI::Frontend::FrameDatas* frameDatas, SDL_Window* sdlWindow)
		{
			stored_window_ = sdlWindow;
			if (!frameDatas)
			{
				std::cout << "[UIManager] syncFrameDatas: frameDatas is null" << std::endl;
				return;
			}

			if (!sdlWindow)
			{
				std::cout << "[UIManager] syncFrameDatas: sdlWindow is null" << std::endl;
				return;
			}

			frameDatas->catchFrameData(sdlWindow);

			const auto& frameDataMap = frameDatas->getFrameData();

			{
				std::lock_guard<std::mutex> lock(ui_panel_mutex_);
				std::map<std::string, IFrameScreenData> preservedGeometry = ui_panel_geometry_frame_data_map_;
				ui_panel_frame_data_map_.clear();
				// Protect frozen geometry: do not clear or overwrite when in fullscreen freeze.
				if (!panel_frames_overridden_by_splitters_ && !coordinates_frozen_for_fullscreen_)
				{
					ui_panel_geometry_frame_data_map_.clear();
				}

				for (const auto& pair : frameDataMap)
				{
					if (pair.first == "viewport_panel")
					{
						continue;
					}

					ui_panel_frame_data_map_[pair.first] = pair.second;
					if (panel_frames_overridden_by_splitters_ || coordinates_frozen_for_fullscreen_)
					{
						// Preserve existing geometry (scaled by splitters or frozen for fullscreen).
						auto preservedIt = preservedGeometry.find(pair.first);
						if (preservedIt != preservedGeometry.end())
						{
							ui_panel_geometry_frame_data_map_[pair.first] = preservedIt->second;
						}
						else
						{
							ui_panel_geometry_frame_data_map_[pair.first] = pair.second;
						}
					}
					else
					{
						ui_panel_geometry_frame_data_map_[pair.first] = pair.second;
					}
				}

				std::cout << "[UIManager] syncFrameDatas: Captured " << ui_panel_frame_data_map_.size() 
						  << " UI panel frames (excluding viewport)" << std::endl;

				for (const auto& pair : ui_panel_frame_data_map_)
				{
					const auto& data = pair.second;
					std::cout << "[UIManager] Panel '" << data.name << "': "
							  << "x=" << data.relativeX << " y=" << data.relativeY
							  << " w=" << data.width << " h=" << data.height
							  << " clientX=" << data.clientX << " clientY=" << data.clientY << std::endl;
				}
			}

			// Do not rebuild splitter_list_ from stale JS data while geometry is frozen for fullscreen.
			if (!ui_panel_frame_data_map_.empty() && !coordinates_frozen_for_fullscreen_)
			{
				int currentW = 0, currentH = 0;
				SDL_GetWindowSize(sdlWindow, &currentW, &currentH);
				panel_map_builder_.syncSplittersAndPanels(
					ui_panel_frame_data_map_,
					currentW, currentH,
					panel_state_map_,
					splitter_list_,
					last_window_width_, last_window_height_,
					sdlWindow);
				splitter_mouse_mecanic_.setAttachments(panel_map_builder_.getMapData().splitterAttachments);
				prev_splitter_list_ = splitter_list_;
			}
		}

		void UIManager::setBorders(int borderLeft, int borderTop, int borderWidth, int borderHeight)
		{
			app_border_left_ = borderLeft;
			app_border_top_ = borderTop;
			app_border_width_ = borderWidth;
			app_border_height_ = borderHeight;
		}

		void UIManager::setCEFTextureForAllPanels(VkImageView view, VkSampler sampler, int cefWidth, int cefHeight)
		{
			if (cefWidth <= 0 || cefHeight <= 0)
				return;
			std::lock_guard<std::mutex> lock(ui_panel_mutex_);
			stored_cef_view_ = view;
			stored_cef_sampler_ = sampler;

			// Always track the actual GPU texture dimensions for UV calculation.
			actual_cef_texture_width_  = cefWidth;
			actual_cef_texture_height_ = cefHeight;

			// Only update stored dims when NOT frozen. While frozen, stored_cef_width_/height_
			// are set to the fullscreen target size by FreezeCoordinatesForFullscreen and must
			// not be overwritten. The freeze is ONLY lifted by ResetFullscreenFreeze() on
			// state transition, never by a CEF paint arriving at any particular size.
			if (!coordinates_frozen_for_fullscreen_)
			{
				stored_cef_width_  = cefWidth;
				stored_cef_height_ = cefHeight;
			}

			pending_geometry_texture_layout_ = true;
		}

		void UIManager::invalidateCEFTexture()
		{
			std::lock_guard<std::mutex> lock(ui_panel_mutex_);
			stored_cef_view_ = VK_NULL_HANDLE;
			stored_cef_sampler_ = VK_NULL_HANDLE;

			if (!coordinates_frozen_for_fullscreen_)
			{
				stored_cef_width_ = 0;
				stored_cef_height_ = 0;
			}
			
			for (auto& pair : ui_panels_)
			{
				if (pair.second)
				{
					pair.second->invalidateExternalTexture();
				}
			}
		}

		bool UIManager::consumePendingCEFRepaintRequest()
		{
			const bool hadPendingRequest = pending_cef_repaint_request_;
			pending_cef_repaint_request_ = false;
			return hadPendingRequest;
		}

		void UIManager::refreshPanelTextureLayout(bool requestCEFRepaint)
		{
			std::lock_guard<std::mutex> lock(ui_panel_mutex_);
			applyPanelTextureLayout(requestCEFRepaint);
		}

		void UIManager::applyPanelTextureLayout(bool requestCEFRepaint)
		{
			if (stored_cef_view_ == VK_NULL_HANDLE || stored_cef_sampler_ == VK_NULL_HANDLE)
			{
				return;
			}

			if (!stored_window_)
			{
				return;
			}

			if (stored_cef_width_ <= 0 || stored_cef_height_ <= 0)
			{
				return;
			}

			const int MIN_PANEL_DIMENSION = 5;
			const float fw = static_cast<float>(stored_cef_width_);
			const float fh = static_cast<float>(stored_cef_height_);

			for (auto& pair : ui_panels_)
			{
				if (!pair.second)
				{
					continue;
				}

				pair.second->PreventClippingForReducedandMaxizimizedWindows(current_window_render_state_);

				// ------------------------- This section is necessary to keep the texture properly aligned the new panel geometry.
				float u0 = 0.0f;
				float v0 = 0.0f;
				float u1 = 1.0f;
				float v1 = 1.0f;

				auto uvIt = ui_panel_frame_data_map_.find(pair.first);
				if (uvIt != ui_panel_frame_data_map_.end())
				{
					u0 = static_cast<float>(uvIt->second.relativeX) / fw;
					v0 = static_cast<float>(uvIt->second.relativeY) / fh;
					u1 = static_cast<float>(uvIt->second.relativeX + uvIt->second.width) / fw;
					v1 = static_cast<float>(uvIt->second.relativeY + uvIt->second.height) / fh;
				}

				auto geometryIt = ui_panel_geometry_frame_data_map_.find(pair.first);
				if (geometryIt != ui_panel_geometry_frame_data_map_.end())
				{
					if (geometryIt->second.width < MIN_PANEL_DIMENSION || geometryIt->second.height < MIN_PANEL_DIMENSION)
					{
						continue;
					}
				}

				pair.second->setCEFTexture(stored_cef_view_, stored_cef_sampler_, u0, v0, u1, v1);
			}

			const auto& effectiveFrameDataMap = ui_panel_geometry_frame_data_map_.empty()
				? ui_panel_frame_data_map_
				: ui_panel_geometry_frame_data_map_;

			panel_map_builder_.attachedUpdatedMap (effectiveFrameDataMap, splitter_list_, current_window_render_state_);

			splitter_mouse_mecanic_.setAttachments(panel_map_builder_.getMapData().splitterAttachments);

			if (requestCEFRepaint)
			{
				std::cout << "[UIManager] applyPanelTextureLayout: Requesting CEF repaint for all panels due to geometry change" << std::endl;
				for (auto& pair : ui_panels_)
				{
					if (!pair.second)
					{
						continue;
					}

					auto geometryIt = ui_panel_geometry_frame_data_map_.find(pair.first);
					if (geometryIt != ui_panel_geometry_frame_data_map_.end())
					{
						if (geometryIt->second.width >= MIN_PANEL_DIMENSION && geometryIt->second.height >= MIN_PANEL_DIMENSION)
						{
							pair.second->RedrawSelfTextureAtCorrectResolution(geometryIt->second.width, geometryIt->second.height);
						}
					}
				}

				pending_cef_repaint_request_ = true;
			}
		}

		void UIManager::drawUIPanelsInsideBorders(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, const std::map<std::string, IFrameScreenData>& panelFrameDataMap, bool skipTextureRebuild, SDL_Window* window)
		{
			if (window) stored_window_ = window;
			std::map<std::string, IFrameScreenData> effectivePanelFrameDataMap;
			{
				std::lock_guard<std::mutex> lock(ui_panel_mutex_);
				if ((panel_frames_overridden_by_splitters_ || splitter_mouse_mecanic_.isOperating()) && !ui_panel_geometry_frame_data_map_.empty())
				{
					effectivePanelFrameDataMap = ui_panel_geometry_frame_data_map_;
				}
				else if (!panelFrameDataMap.empty())
				{
					effectivePanelFrameDataMap = panelFrameDataMap;
				}
				else
				{
					effectivePanelFrameDataMap = ui_panel_geometry_frame_data_map_;
				}
			}

			if (effectivePanelFrameDataMap.empty())
			{
				return;
			}

			panel_map_builder_.drawInsideAppBorders(commandBuffer, drawableWidth, drawableHeight,
				effectivePanelFrameDataMap, skipTextureRebuild, vk_context_, vulkan_pipelines_,
				vk_render_pass_, ui_panels_, window,
				app_border_left_, app_border_top_, app_border_width_, app_border_height_);
		}

		void UIManager::drawSplitters(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight)
		{
			if (splitter_list_.empty() || !vk_context_ || !vulkan_pipelines_ || vk_render_pass_ == VK_NULL_HANDLE)
			{
				return;
			}

			if (!splitter_renderer_)
			{
				splitter_renderer_ = std::make_unique<Splitter>();
				splitter_renderer_->setVulkanPipelines(vulkan_pipelines_);
				splitter_renderer_->setSplitterMouseMecanic(&splitter_mouse_mecanic_);
				splitter_renderer_->initialize(vk_context_, vk_render_pass_);
			}

			std::vector<Splitter::SplitterData> splitterData;
			splitterData.reserve(splitter_list_.size());
			for (const auto& def : splitter_list_)
			{
				Splitter::SplitterData sd;
				sd.x = def.x;
				sd.y = def.y;
				sd.width = def.width;
				sd.height = def.height;
				sd.isVertical = def.isVertical;
				sd.isHorizontal = def.isHorizontal;
				splitterData.push_back(sd);
			}

			splitter_renderer_->setSplitters(splitterData);
			splitter_renderer_->draw(commandBuffer, drawableWidth, drawableHeight);
		}

		void UIManager::drawSplittersFullScreen(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, SDL_Window* window, int referenceWindowWidth, int referenceWindowHeight)
		{
			if (splitter_list_.empty() || !vk_context_ || !vulkan_pipelines_ || vk_render_pass_ == VK_NULL_HANDLE)
			{
				return;
			}

			if (referenceWindowWidth <= 0 || referenceWindowHeight <= 0 || !window)
			{
				return;
			}

			int currentLogicalW = 0, currentLogicalH = 0;
			SDL_GetWindowSize(window, &currentLogicalW, &currentLogicalH);
			if (currentLogicalW <= 0 || currentLogicalH <= 0)
			{
				return;
			}

			float scaleX = static_cast<float>(currentLogicalW) / static_cast<float>(referenceWindowWidth);
			float scaleY = static_cast<float>(currentLogicalH) / static_cast<float>(referenceWindowHeight);
			const bool splittersAlreadyMatchWindow = last_window_width_ == currentLogicalW && last_window_height_ == currentLogicalH;

			if (!splitter_renderer_)
			{
				splitter_renderer_ = std::make_unique<Splitter>();
				splitter_renderer_->setVulkanPipelines(vulkan_pipelines_);
				splitter_renderer_->setSplitterMouseMecanic(&splitter_mouse_mecanic_);
				splitter_renderer_->initialize(vk_context_, vk_render_pass_);
			}

			std::vector<Splitter::SplitterData> splitterData;
			splitterData.reserve(splitter_list_.size());
			for (const auto& def : splitter_list_)
			{
				Splitter::SplitterData sd;
				if (splittersAlreadyMatchWindow)
				{
					sd.x = def.x;
					sd.y = def.y;
					sd.width = def.width;
					sd.height = def.height;
				}
				else
				{
					sd.x = static_cast<int>(std::round(def.x * scaleX));
					sd.y = static_cast<int>(std::round(def.y * scaleY));
					sd.width = static_cast<int>(std::round(def.width  * scaleX));
					sd.height = static_cast<int>(std::round(def.height * scaleY));
				}
				sd.isVertical = def.isVertical;
				sd.isHorizontal = def.isHorizontal;
				splitterData.push_back(sd);
			}

			splitter_renderer_->setSplitters(splitterData);
			splitter_renderer_->draw(commandBuffer, drawableWidth, drawableHeight);
		}

		void UIManager::drawSplittersReducedScreenSize(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, SDL_Window* window, int referenceWindowWidth, int referenceWindowHeight)
		{
			if (splitter_list_.empty() || !vk_context_ || !vulkan_pipelines_ || vk_render_pass_ == VK_NULL_HANDLE)
			{
				return;
			}

			if (referenceWindowWidth <= 0 || referenceWindowHeight <= 0 || !window)
			{
				return;
			}

			int currentLogicalW = 0, currentLogicalH = 0;
			SDL_GetWindowSize(window, &currentLogicalW, &currentLogicalH);
			if (currentLogicalW <= 0 || currentLogicalH <= 0)
			{
				return;
			}

			float scaleX = static_cast<float>(currentLogicalW) / static_cast<float>(referenceWindowWidth);
			float scaleY = static_cast<float>(currentLogicalH) / static_cast<float>(referenceWindowHeight);
			const bool splittersAlreadyMatchWindow = last_window_width_ == currentLogicalW && last_window_height_ == currentLogicalH;

			if (!splitter_renderer_)
			{
				splitter_renderer_ = std::make_unique<Splitter>();
				splitter_renderer_->setVulkanPipelines(vulkan_pipelines_);
				splitter_renderer_->setSplitterMouseMecanic(&splitter_mouse_mecanic_);
				splitter_renderer_->initialize(vk_context_, vk_render_pass_);
			}

			std::vector<Splitter::SplitterData> splitterData;
			splitterData.reserve(splitter_list_.size());
			for (const auto& def : splitter_list_)
			{
				Splitter::SplitterData sd;
				if (splittersAlreadyMatchWindow)
				{
					sd.x = def.x;
					sd.y = def.y;
					sd.width = def.width;
					sd.height = def.height;
				}
				else
				{
					sd.x = static_cast<int>(std::round(def.x * scaleX));
					sd.y = static_cast<int>(std::round(def.y * scaleY));
					sd.width = static_cast<int>(std::round(def.width * scaleX));
					sd.height = static_cast<int>(std::round(def.height * scaleY));
				}
				sd.isVertical = def.isVertical;
				sd.isHorizontal = def.isHorizontal;
				splitterData.push_back(sd);
			}

			splitter_renderer_->setSplitters(splitterData);
			splitter_renderer_->draw(commandBuffer, drawableWidth, drawableHeight);
		}

		std::vector<SplitterDefinition> UIManager::scaleSplittersToWindow(int targetWindowWidth, int targetWindowHeight, int referenceWindowWidth, int referenceWindowHeight) const
		{
			std::vector<SplitterDefinition> scaledSplitters;
			scaledSplitters.reserve(splitter_list_.size());

			if (targetWindowWidth <= 0 || targetWindowHeight <= 0)
			{
				return scaledSplitters;
			}

			const bool splittersAlreadyMatchWindow = last_window_width_ == targetWindowWidth && last_window_height_ == targetWindowHeight;
			const float scaleX = referenceWindowWidth > 0
				? static_cast<float>(targetWindowWidth) / static_cast<float>(referenceWindowWidth)
				: 1.0f;
			const float scaleY = referenceWindowHeight > 0
				? static_cast<float>(targetWindowHeight) / static_cast<float>(referenceWindowHeight)
				: 1.0f;

			for (const auto& splitter : splitter_list_)
			{
				SplitterDefinition scaled = splitter;
				if (!splittersAlreadyMatchWindow)
				{
					scaled.x = static_cast<int>(std::round(splitter.x * scaleX));
					scaled.y = static_cast<int>(std::round(splitter.y * scaleY));
					scaled.width = static_cast<int>(std::round(splitter.width * scaleX));
					scaled.height = static_cast<int>(std::round(splitter.height * scaleY));
				}
				scaledSplitters.push_back(scaled);
			}

			return scaledSplitters;
		}

		void UIManager::drawReduceScreenUIpanelsInsideBorders(
			VkCommandBuffer commandBuffer,
			int drawableWidth, int drawableHeight,
			const std::map<std::string, IFrameScreenData>& panelFrameDataMap,
			bool skipTextureRebuild, SDL_Window* window,
			int referenceWindowWidth, int referenceWindowHeight)
		{
			if (referenceWindowWidth <= 0 || referenceWindowHeight <= 0 || !window)
			{
				return;
			}

			int currentLogicalW = 0, currentLogicalH = 0;
			SDL_GetWindowSize(window, &currentLogicalW, &currentLogicalH);

			if (currentLogicalW <= 0 || currentLogicalH <= 0)
			{
				return;
			}

			float scaleX = static_cast<float>(currentLogicalW) / static_cast<float>(referenceWindowWidth);
			float scaleY = static_cast<float>(currentLogicalH) / static_cast<float>(referenceWindowHeight);

			std::map<std::string, IFrameScreenData> sourceFrameDataMap;
			{
				std::lock_guard<std::mutex> lock(ui_panel_mutex_);
				if ((panel_frames_overridden_by_splitters_ || splitter_mouse_mecanic_.isOperating()) && !ui_panel_geometry_frame_data_map_.empty())
				{
					sourceFrameDataMap = ui_panel_geometry_frame_data_map_;
				}
				else if (!panelFrameDataMap.empty())
				{
					sourceFrameDataMap = panelFrameDataMap;
				}
				else
				{
					sourceFrameDataMap = ui_panel_geometry_frame_data_map_;
				}
			}

			if (sourceFrameDataMap.empty())
			{
				return;
			}

			if (frameMapMatchesWindowSize(sourceFrameDataMap, currentLogicalW, currentLogicalH))
			{
				static constexpr int BORDER = 3;
				int borderLeft = BORDER;
				int borderTop = BORDER;
				int borderWidth = drawableWidth - 2 * BORDER;
				int borderHeight = drawableHeight - 2 * BORDER;

				panel_map_builder_.drawInsideAppBorders(
					commandBuffer, drawableWidth, drawableHeight,
					sourceFrameDataMap, skipTextureRebuild,
					vk_context_, vulkan_pipelines_, vk_render_pass_,
					ui_panels_, window,
					borderLeft, borderTop, borderWidth, borderHeight);
				return;
			}

			std::map<std::string, IFrameScreenData> scaledFrameDataMap;

			for (const auto& pair : sourceFrameDataMap)
			{
				IFrameScreenData scaled = pair.second;
				scaled.relativeX = static_cast<int>(std::round(pair.second.relativeX * scaleX));
				scaled.relativeY = static_cast<int>(std::round(pair.second.relativeY * scaleY));
				scaled.width = static_cast<int>(std::round(pair.second.width * scaleX));
				scaled.height = static_cast<int>(std::round(pair.second.height * scaleY));
				scaled.clientX = static_cast<int>(std::round(pair.second.clientX * scaleX));
				scaled.clientY = static_cast<int>(std::round(pair.second.clientY * scaleY));
				scaledFrameDataMap[pair.first] = scaled;
			}

			static constexpr int BORDER = 3;
			int borderLeft = BORDER;
			int borderTop = BORDER;
			int borderWidth = drawableWidth - 2 * BORDER;
			int borderHeight = drawableHeight - 2 * BORDER;

			panel_map_builder_.drawInsideAppBorders(
				commandBuffer, drawableWidth, drawableHeight,
				scaledFrameDataMap, skipTextureRebuild,
				vk_context_, vulkan_pipelines_, vk_render_pass_,
				ui_panels_, window,
				borderLeft, borderTop, borderWidth, borderHeight);
		}

		void UIManager::enableSplitterMouseInteractions(int mouseX, int mouseY, bool isLeftButtonDown)
		{
			// ------------ push color based on mouse postion for splitters

			if (!splitter_renderer_ || splitter_list_.empty())
			{
				return;
			}

			std::vector<Splitter::SplitterData> splitterData;
			splitterData.reserve(splitter_list_.size());

			for (const auto& def : splitter_list_)
			{
				Splitter::SplitterData sd;
				sd.x = def.x;
				sd.y = def.y;
				sd.width = def.width;
				sd.height = def.height;
				sd.isVertical = def.isVertical;
				sd.isHorizontal = def.isHorizontal;
				splitterData.push_back(sd);
			}

			splitter_mouse_mecanic_.update(splitterData, mouseX, mouseY);
			splitter_renderer_->setHoveredIndex(splitter_mouse_mecanic_.getHoveredIndex());

			dragSplitter(mouseX, mouseY, isLeftButtonDown);
		}

		void UIManager::dragSplitter(int mouseX, int mouseY, bool isLeftButtonDown)
		{
			if (!splitter_renderer_ || splitter_list_.empty())
			{
				return;
			}

			std::vector<Splitter::SplitterData> splitterData;
			splitterData.reserve(splitter_list_.size());

			for (const auto& def : splitter_list_)
			{
				Splitter::SplitterData sd;
				sd.x = def.x;
				sd.y = def.y;
				sd.width = def.width;
				sd.height = def.height;
				sd.isVertical = def.isVertical;
				sd.isHorizontal = def.isHorizontal;
				splitterData.push_back(sd);
			}

			splitter_mouse_mecanic_.dragSplitter(splitterData, mouseX, mouseY, isLeftButtonDown);

			for (std::size_t i = 0; i < splitter_list_.size() && i < splitterData.size(); ++i)
			{
				splitter_list_[i].x = splitterData[i].x;
				splitter_list_[i].y = splitterData[i].y;
				splitter_list_[i].width = splitterData[i].width;
				splitter_list_[i].height = splitterData[i].height;
			}
		}

		void UIManager::bindWorkSpaceSizeToSplitterInteractions()
		{
			if (splitter_list_.empty())
				return;

			panel_map_builder_.updateWorkSpaceFromSplitters(splitter_list_, last_window_width_, last_window_height_);
		}

		void UIManager::bindPanelsToSplitters(WindowRenderState currentWindowState)
		{
			current_window_render_state_ = currentWindowState;

			if (splitter_list_.empty())
			{
				if (pending_geometry_texture_layout_)
				{
					std::lock_guard<std::mutex> lock(ui_panel_mutex_);
					applyPanelTextureLayout(false);
					pending_geometry_texture_layout_ = false;
				}
				return;
			}

			const bool isOperating = splitter_mouse_mecanic_.isOperating();
			if (isOperating)
			{
				std::cout << "[UIManager] Splitter operating: " << (isOperating ? "true" : "false") << std::endl;
			}
			else if (isOperating != was_splitter_operating_)
			{
				std::cout << "[UIManager] Splitter operating: false" << std::endl;
			}

			if (prev_splitter_list_.size() != splitter_list_.size())
			{
				prev_splitter_list_ = splitter_list_;
				was_splitter_operating_ = isOperating;
			}

			const bool hasJustStoppedOperating = was_splitter_operating_ && !isOperating;
			const int SIDE_TOLERANCE = 20;
			const int MIN_PANEL_SIZE = 10;
			bool panelGeometryChanged = false;
			int currentLogicalW = 0;
			int currentLogicalH = 0;
			float currentDpiScale = 1.0f;

			if (stored_window_)
			{
				SDL_GetWindowSize(stored_window_, &currentLogicalW, &currentLogicalH);
				int currentDrawableW = 0;
				int currentDrawableH = 0;
				SDL_GetWindowSizeInPixels(stored_window_, &currentDrawableW, &currentDrawableH);
				if (currentLogicalW > 0 && currentDrawableW > 0)
				{
					currentDpiScale = static_cast<float>(currentDrawableW) / static_cast<float>(currentLogicalW);
				}
			}

			std::lock_guard<std::mutex> lock(ui_panel_mutex_);

			// tried this stuff to normalize the splitter geometry
			if (isOperating && currentLogicalW > 0 && currentLogicalH > 0 && !ui_panel_geometry_frame_data_map_.empty())
			{
				int sourceWindowWidth = 0;
				int sourceWindowHeight = 0;

				for (const auto& panelPair : ui_panel_geometry_frame_data_map_)
				{
					const IFrameScreenData& frame = panelPair.second;
					if (frame.windowWidth > 0 && frame.windowHeight > 0)
					{
						sourceWindowWidth = frame.windowWidth;
						sourceWindowHeight = frame.windowHeight;
						break;
					}
				}

				if (sourceWindowWidth > 0 && sourceWindowHeight > 0 &&
					(sourceWindowWidth != currentLogicalW || sourceWindowHeight != currentLogicalH))
				{
					const float normalizeScaleX = static_cast<float>(currentLogicalW) / static_cast<float>(sourceWindowWidth);
					const float normalizeScaleY = static_cast<float>(currentLogicalH) / static_cast<float>(sourceWindowHeight);
					int currentWindowX = 0;
					int currentWindowY = 0;

					if (stored_window_)
					{
						SDL_GetWindowPosition(stored_window_, &currentWindowX, &currentWindowY);
					}

					std::cout << "[UIManager] Normalizing splitter geometry from "
						      << sourceWindowWidth << "x" << sourceWindowHeight
						      << " to " << currentLogicalW << "x" << currentLogicalH
						      << " before live drag" << std::endl;

					for (auto& panelPair : ui_panel_geometry_frame_data_map_)
					{
						IFrameScreenData& frame = panelPair.second;
						const int clientOffsetX = frame.clientX - frame.relativeX;
						const int clientOffsetY = frame.clientY - frame.relativeY;
						frame.relativeX = static_cast<int>(std::round(static_cast<float>(frame.relativeX) * normalizeScaleX));
						frame.relativeY = static_cast<int>(std::round(static_cast<float>(frame.relativeY) * normalizeScaleY));
						frame.width = static_cast<int>(std::round(static_cast<float>(frame.width) * normalizeScaleX));
						frame.height = static_cast<int>(std::round(static_cast<float>(frame.height) * normalizeScaleY));
						frame.clientX = frame.relativeX + clientOffsetX;
						frame.clientY = frame.relativeY + clientOffsetY;
						frame.windowX = currentWindowX;
						frame.windowY = currentWindowY;
						frame.screenX = currentWindowX + frame.clientX;
						frame.screenY = currentWindowY + frame.clientY;
						frame.windowWidth = currentLogicalW;
						frame.windowHeight = currentLogicalH;
						frame.dpiScale = currentDpiScale;
					}
				}
			}

			if (currentLogicalW > 0 && currentLogicalH > 0 &&
				(last_window_width_ != currentLogicalW || last_window_height_ != currentLogicalH))
			{
				const std::map<std::string, IFrameScreenData>* rebuildFrameDataMap = nullptr;

				if (frameMapMatchesWindowSize(ui_panel_geometry_frame_data_map_, currentLogicalW, currentLogicalH))
				{
					rebuildFrameDataMap = &ui_panel_geometry_frame_data_map_;
				}
				else if (frameMapMatchesWindowSize(ui_panel_frame_data_map_, currentLogicalW, currentLogicalH))
				{
					rebuildFrameDataMap = &ui_panel_frame_data_map_;
				}

				if (rebuildFrameDataMap)
				{
					std::cout << "[UIManager] Rebuilding splitter map for current SDL window size "
						      << currentLogicalW << "x" << currentLogicalH
						      << " before continuing splitter interaction" << std::endl;

					panel_map_builder_.syncSplittersAndPanels(
						*rebuildFrameDataMap,
						currentLogicalW,
						currentLogicalH,
						panel_state_map_,
						splitter_list_,
						last_window_width_,
						last_window_height_,
						stored_window_);

					splitter_mouse_mecanic_.setAttachments(panel_map_builder_.getMapData().splitterAttachments);
					prev_splitter_list_ = splitter_list_;
				}
			}

			for (std::size_t i = 0; i < splitter_list_.size(); ++i)
			{
				const SplitterDefinition& current = splitter_list_[i];
				const SplitterDefinition& previous = prev_splitter_list_[i];

				const int dx = current.x - previous.x;
				const int dy = current.y - previous.y;

				if (dx == 0 && dy == 0)
					continue;

				const int previousSplitterLeft = previous.x;
				const int previousSplitterRight = previous.x + previous.width;
				const int currentSplitterLeft = current.x;
				const int currentSplitterRight = current.x + current.width;
				const int previousSplitterTop = previous.y;
				const int previousSplitterBottom = previous.y + previous.height;
				const int currentSplitterTop = current.y;
				const int currentSplitterBottom = current.y + current.height;

				for (auto& panelPair : ui_panel_geometry_frame_data_map_)
				{
					IFrameScreenData& fd = panelPair.second;
					const int originalLeft = fd.relativeX;
					const int originalTop = fd.relativeY;
					const int originalRight = fd.relativeX + fd.width;
					const int originalBottom = fd.relativeY + fd.height;

					if (current.isVertical && dx != 0)
					{
						if (std::abs(originalRight - previousSplitterLeft) <= SIDE_TOLERANCE)
						{
							const int newWidth = (std::max)(MIN_PANEL_SIZE, currentSplitterLeft - fd.relativeX);
							panelGeometryChanged = panelGeometryChanged || (newWidth != fd.width);
							fd.width = newWidth;
						}
						else if (std::abs(originalLeft - previousSplitterRight) <= SIDE_TOLERANCE)
						{
							const int newRelativeX = currentSplitterRight;
							const int newClientX = fd.clientX + (newRelativeX - fd.relativeX);
							const int newWidth = (std::max)(MIN_PANEL_SIZE, originalRight - newRelativeX);
							panelGeometryChanged = panelGeometryChanged ||
								(newRelativeX != fd.relativeX) ||
								(newClientX != fd.clientX) ||
								(newWidth != fd.width);
							fd.relativeX = newRelativeX;
							fd.clientX = newClientX;
							fd.width = newWidth;
						}
					}
					else if (current.isHorizontal && dy != 0)
					{
						if (std::abs(originalBottom - previousSplitterTop) <= SIDE_TOLERANCE)
						{
							const int newHeight = (std::max)(MIN_PANEL_SIZE, currentSplitterTop - fd.relativeY);
							panelGeometryChanged = panelGeometryChanged || (newHeight != fd.height);
							fd.height = newHeight;
						}
						else if (std::abs(originalTop - previousSplitterBottom) <= SIDE_TOLERANCE)
						{
							const int newRelativeY = currentSplitterBottom;
							const int newClientY = fd.clientY + (newRelativeY - fd.relativeY);
							const int newHeight = (std::max)(MIN_PANEL_SIZE, originalBottom - newRelativeY);
							panelGeometryChanged = panelGeometryChanged ||
								(newRelativeY != fd.relativeY) ||
								(newClientY != fd.clientY) ||
								(newHeight != fd.height);
							fd.relativeY = newRelativeY;
							fd.clientY   = newClientY;
							fd.height = newHeight;
						}
					}
				}
			}

			prev_splitter_list_ = splitter_list_;
			if (panelGeometryChanged)
			{
				if (currentLogicalW > 0 && currentLogicalH > 0)
				{
					for (auto& panelPair : ui_panel_geometry_frame_data_map_)
					{
						panelPair.second.windowWidth = currentLogicalW;
						panelPair.second.windowHeight = currentLogicalH;
						panelPair.second.dpiScale = currentDpiScale;
					}
				}

				panel_frames_overridden_by_splitters_ = true;
				if (isOperating)
				{
					pending_geometry_texture_layout_ = true;
				}
			}

			if (hasJustStoppedOperating)
			{
				pending_geometry_texture_layout_ = true;
			}

			if (pending_geometry_texture_layout_)
			{
				applyPanelTextureLayout(hasJustStoppedOperating);
				pending_geometry_texture_layout_ = false;
			}

			was_splitter_operating_ = isOperating;
		}

		void UIManager::applyFullScreenPanelTextureLayout(bool requestCEFRepaint, int fullscreenWidth, int fullscreenHeight)
		{
			if (stored_cef_view_ == VK_NULL_HANDLE || stored_cef_sampler_ == VK_NULL_HANDLE)
			{
				return;
			}

			if (!stored_window_)
			{
				return;
			}

			if (stored_cef_width_ <= 0 || stored_cef_height_ <= 0)
			{
				return;
			}

			if (fullscreenWidth <= 0 || fullscreenHeight <= 0)
			{
				return;
			}

			const int refW = stored_cef_width_;
			const int refH = stored_cef_height_;
			const float scaleX = static_cast<float>(fullscreenWidth) / static_cast<float>(refW);
			const float scaleY = static_cast<float>(fullscreenHeight) / static_cast<float>(refH);

			const int MIN_PANEL_DIMENSION = 5;
			// Use actual GPU texture dimensions for UV calculation, not the frozen logical size.
			// When freeze is active, stored_cef_width_ may be 2560 while the texture is still 1920.
			const float fw = static_cast<float>(actual_cef_texture_width_ > 0 ? actual_cef_texture_width_ : stored_cef_width_);
			const float fh = static_cast<float>(actual_cef_texture_height_ > 0 ? actual_cef_texture_height_ : stored_cef_height_);

			for (auto& pair : ui_panels_)
			{
				if (!pair.second)
				{
					continue;
				}

				pair.second->PreventClippingForReducedandMaxizimizedWindows(current_window_render_state_);

				float u0 = 0.0f;
				float v0 = 0.0f;
				float u1 = 1.0f;
				float v1 = 1.0f;

				auto uvIt = ui_panel_frame_data_map_.find(pair.first);
				if (uvIt != ui_panel_frame_data_map_.end())
				{
					u0 = static_cast<float>(uvIt->second.relativeX) / fw;
					v0 = static_cast<float>(uvIt->second.relativeY) / fh;
					u1 = static_cast<float>(uvIt->second.relativeX + uvIt->second.width) / fw;
					v1 = static_cast<float>(uvIt->second.relativeY + uvIt->second.height) / fh;
				}

				auto geometryIt = ui_panel_geometry_frame_data_map_.find(pair.first);
				if (geometryIt != ui_panel_geometry_frame_data_map_.end())
				{
					if (geometryIt->second.width < MIN_PANEL_DIMENSION || geometryIt->second.height < MIN_PANEL_DIMENSION)
					{
						continue;
					}
				}

				pair.second->setCEFTexture(stored_cef_view_, stored_cef_sampler_, u0, v0, u1, v1);
			}

			const auto& effectiveFrameDataMap = ui_panel_geometry_frame_data_map_.empty()
				? ui_panel_frame_data_map_
				: ui_panel_geometry_frame_data_map_;

			std::map<std::string, IFrameScreenData> scaledFrameDataMap;
			for (const auto& p : effectiveFrameDataMap)
			{
				IFrameScreenData scaled = p.second;
				scaled.relativeX = static_cast<int>(std::round(p.second.relativeX * scaleX));
				scaled.relativeY = static_cast<int>(std::round(p.second.relativeY * scaleY));
				scaled.width  = static_cast<int>(std::round(p.second.width * scaleX));
				scaled.height  = static_cast<int>(std::round(p.second.height * scaleY));
				scaled.clientX = static_cast<int>(std::round(p.second.clientX * scaleX));
				scaled.clientY = static_cast<int>(std::round(p.second.clientY * scaleY));
				scaled.windowWidth = fullscreenWidth;
				scaled.windowHeight = fullscreenHeight;
				scaledFrameDataMap[p.first] = scaled;
			}

			const std::vector<SplitterDefinition> scaledSplitters = scaleSplittersToWindow(fullscreenWidth, fullscreenHeight, refW, refH);

			panel_map_builder_.attachedUpdatedMap(scaledFrameDataMap, scaledSplitters, current_window_render_state_);

			splitter_mouse_mecanic_.setAttachments(panel_map_builder_.getMapData().splitterAttachments);

			if (requestCEFRepaint)
			{
				for (auto& pair : ui_panels_)
				{
					if (!pair.second)
					{
						continue;
					}

					auto geometryIt = ui_panel_geometry_frame_data_map_.find(pair.first);
					if (geometryIt != ui_panel_geometry_frame_data_map_.end())
					{
						const int scaledW = static_cast<int>(std::round(geometryIt->second.width * scaleX));
						const int scaledH = static_cast<int>(std::round(geometryIt->second.height * scaleY));
						if (scaledW >= MIN_PANEL_DIMENSION && scaledH >= MIN_PANEL_DIMENSION)
						{
							pair.second->RedrawSelfTextureAtCorrectResolution(scaledW, scaledH);
						}
					}
				}

				pending_cef_repaint_request_ = true;
			}
		}

		void UIManager::ResetFullscreenFreeze()
		{
			if (!coordinates_frozen_for_fullscreen_)
				return;

			std::map<std::string, IFrameScreenData> localFrameDataMap;

			{
				std::lock_guard<std::mutex> lock(ui_panel_mutex_);

				coordinates_frozen_for_fullscreen_ = false;
				frozen_fullscreen_width_ = 0;
				frozen_fullscreen_height_ = 0;
				panel_frames_overridden_by_splitters_ = false;

				// Restore geometry map to the original JS 1920-space data.
				ui_panel_geometry_frame_data_map_ = ui_panel_frame_data_map_;

				// Extract JS window size so syncSplittersAndPanels sees lastW == currentW.
				for (const auto& pair : ui_panel_frame_data_map_)
				{
					if (pair.second.windowWidth > 0 && pair.second.windowHeight > 0)
					{
						last_window_width_ = pair.second.windowWidth;
						last_window_height_ = pair.second.windowHeight;
						break;
					}
				}

				panel_state_map_.clear();
				splitter_list_.clear();
				prev_splitter_list_.clear();
				pending_geometry_texture_layout_ = true;

				localFrameDataMap = ui_panel_frame_data_map_;
			}

			// Immediately rebuild splitters from 1920-space JS data so they appear
			// on the very next frame without waiting for a new syncFrameDatas call.
			// (JS won't resend unchanged data, so splitter_list_ would stay empty.)
			if (!localFrameDataMap.empty() && stored_window_)
			{
				panel_map_builder_.syncSplittersAndPanels(
					localFrameDataMap,
					last_window_width_, last_window_height_,
					panel_state_map_,
					splitter_list_,
					last_window_width_, last_window_height_,
					stored_window_);
				splitter_mouse_mecanic_.setAttachments(panel_map_builder_.getMapData().splitterAttachments);
				prev_splitter_list_ = splitter_list_;
			}
		}

		void UIManager::FreezeCoordinatesForFullscreen(int fullscreenWidth, int fullscreenHeight)
		{
			if (coordinates_frozen_for_fullscreen_)
				return;

			if (stored_cef_width_ <= 0 || stored_cef_height_ <= 0)
				return;

			if (fullscreenWidth <= 0 || fullscreenHeight <= 0)
				return;

			if (stored_cef_width_ == fullscreenWidth && stored_cef_height_ == fullscreenHeight)
			{
				coordinates_frozen_for_fullscreen_ = true;
				frozen_fullscreen_width_ = fullscreenWidth;
				frozen_fullscreen_height_ = fullscreenHeight;
				return;
			}

			const float scaleX = static_cast<float>(fullscreenWidth)  / static_cast<float>(stored_cef_width_);
			const float scaleY = static_cast<float>(fullscreenHeight) / static_cast<float>(stored_cef_height_);

			{
				std::lock_guard<std::mutex> lock(ui_panel_mutex_);

				for (auto& pair : ui_panel_geometry_frame_data_map_)
				{
					IFrameScreenData& fd = pair.second;
					fd.relativeX  = static_cast<int>(std::round(fd.relativeX * scaleX));
					fd.relativeY = static_cast<int>(std::round(fd.relativeY * scaleY));
					fd.width = static_cast<int>(std::round(fd.width     * scaleX));
					fd.height = static_cast<int>(std::round(fd.height    * scaleY));
					fd.clientX  = static_cast<int>(std::round(fd.clientX   * scaleX));
					fd.clientY = static_cast<int>(std::round(fd.clientY   * scaleY));
					fd.windowWidth  = fullscreenWidth;
					fd.windowHeight = fullscreenHeight;
				}

				for (auto& splitter : splitter_list_)
				{
					splitter.x = static_cast<int>(std::round(splitter.x * scaleX));
					splitter.y = static_cast<int>(std::round(splitter.y * scaleY));
					splitter.width  = static_cast<int>(std::round(splitter.width  * scaleX));
					splitter.height = static_cast<int>(std::round(splitter.height * scaleY));
				}
				prev_splitter_list_ = splitter_list_;

				stored_cef_width_ = fullscreenWidth;
				stored_cef_height_ = fullscreenHeight;

				// Inform drawSplittersFullScreen that splitter_list_ is now in fullscreen space.

				last_window_width_ = fullscreenWidth;
				last_window_height_ = fullscreenHeight;
			}

			coordinates_frozen_for_fullscreen_ = true;
			frozen_fullscreen_width_ = fullscreenWidth;
			frozen_fullscreen_height_ = fullscreenHeight;
		}

		void UIManager::bindFullScreenPanelsToSplitters(WindowRenderState currentWindowState)
		{
			current_window_render_state_ = currentWindowState;

			if (!stored_window_)
			{
				return;
			}

			int fullscreenW = 0, fullscreenH = 0;
			SDL_GetWindowSize(stored_window_, &fullscreenW, &fullscreenH);

			if (fullscreenW <= 0 || fullscreenH <= 0)
			{
				return;
			}

			if (splitter_list_.empty())
			{
				if (pending_geometry_texture_layout_)
				{
					std::lock_guard<std::mutex> lock(ui_panel_mutex_);
					applyFullScreenPanelTextureLayout(false, fullscreenW, fullscreenH);
					pending_geometry_texture_layout_ = false;
				}
				return;
			}

			const bool isOperating = splitter_mouse_mecanic_.isOperating();

			if (prev_splitter_list_.size() != splitter_list_.size())
			{
				prev_splitter_list_ = splitter_list_;
				was_splitter_operating_ = isOperating;
			}

			const bool hasJustStoppedOperating = was_splitter_operating_ && !isOperating;

			const int refW = (stored_cef_width_ > 0) ? stored_cef_width_ : fullscreenW;
			const int refH = (stored_cef_height_ > 0) ? stored_cef_height_ : fullscreenH;
			const float scaleX = static_cast<float>(fullscreenW) / static_cast<float>(refW);
			const float scaleY = static_cast<float>(fullscreenH) / static_cast<float>(refH);

			const int SIDE_TOLERANCE = 20;
			const int MIN_PANEL_SIZE = 10;
			bool panelGeometryChanged = false;

			std::lock_guard<std::mutex> lock(ui_panel_mutex_);

			for (std::size_t i = 0; i < splitter_list_.size(); ++i)
			{
				const SplitterDefinition& current = splitter_list_[i];
				const SplitterDefinition& previous = prev_splitter_list_[i];

				const int dx = current.x - previous.x;
				const int dy = current.y - previous.y;

				if (dx == 0 && dy == 0)
					continue;

				const int prevSplitterLeft = previous.x;
				const int prevSplitterRight  = previous.x + previous.width;
				const int currSplitterLeft = current.x;
				const int currSplitterRight = current.x + current.width;
				const int prevSplitterTop  = previous.y;
				const int prevSplitterBottom = previous.y + previous.height;
				const int currSplitterTop  = current.y;
				const int currSplitterBottom = current.y + current.height;

				for (auto& panelPair : ui_panel_geometry_frame_data_map_)
				{
					IFrameScreenData& fd = panelPair.second;
					const int originalLeft = fd.relativeX;
					const int originalTop  = fd.relativeY;
					const int originalRight  = fd.relativeX + fd.width;
					const int originalBottom = fd.relativeY + fd.height;

					if (current.isVertical && dx != 0)
					{
						if (std::abs(originalRight - prevSplitterLeft) <= SIDE_TOLERANCE)
						{
							const int newWidth = (std::max)(MIN_PANEL_SIZE, currSplitterLeft - fd.relativeX);
							panelGeometryChanged = panelGeometryChanged || (newWidth != fd.width);
							fd.width = newWidth;
						}
						else if (std::abs(originalLeft - prevSplitterRight) <= SIDE_TOLERANCE)
						{
							const int newRelativeX = currSplitterRight;
							const int newClientX   = fd.clientX + (newRelativeX - fd.relativeX);
							const int newWidth     = (std::max)(MIN_PANEL_SIZE, originalRight - newRelativeX);
							panelGeometryChanged = panelGeometryChanged ||
								(newRelativeX != fd.relativeX) || (newClientX != fd.clientX) || (newWidth != fd.width);
							fd.relativeX = newRelativeX;
							fd.clientX   = newClientX;
							fd.width     = newWidth;
						}
					}
					else if (current.isHorizontal && dy != 0)
					{
						if (std::abs(originalBottom - prevSplitterTop) <= SIDE_TOLERANCE)
						{
							const int newHeight = (std::max)(MIN_PANEL_SIZE, currSplitterTop - fd.relativeY);
							panelGeometryChanged = panelGeometryChanged || (newHeight != fd.height);
							fd.height = newHeight;
						}
						else if (std::abs(originalTop - prevSplitterBottom) <= SIDE_TOLERANCE)
						{
							const int newRelativeY = currSplitterBottom;
							const int newClientY   = fd.clientY + (newRelativeY - fd.relativeY);
							const int newHeight    = (std::max)(MIN_PANEL_SIZE, originalBottom - newRelativeY);
							panelGeometryChanged = panelGeometryChanged ||
								(newRelativeY != fd.relativeY) || (newClientY != fd.clientY) || (newHeight != fd.height);
							fd.relativeY = newRelativeY;
							fd.clientY   = newClientY;
							fd.height    = newHeight;
						}
					}
				}
			}

			prev_splitter_list_ = splitter_list_;

			if (panelGeometryChanged)
			{
				panel_frames_overridden_by_splitters_ = true;
				if (isOperating)
				{
					pending_geometry_texture_layout_ = true;
				}
			}

			if (hasJustStoppedOperating)
			{
				pending_geometry_texture_layout_ = true;
			}

			if (pending_geometry_texture_layout_)
			{
				applyFullScreenPanelTextureLayout(hasJustStoppedOperating, fullscreenW, fullscreenH);
				pending_geometry_texture_layout_ = false;
			}

			was_splitter_operating_ = isOperating;
		}

		void UIManager::drawFullScreenUIPanelsInsideBorders(
			VkCommandBuffer commandBuffer,
			int drawableWidth, int drawableHeight,
			const std::map<std::string, IFrameScreenData>& panelFrameDataMap,
			bool skipTextureRebuild, SDL_Window* window,
			int referenceWindowWidth, int referenceWindowHeight)
		{

			if (referenceWindowWidth <= 0 || referenceWindowHeight <= 0 || !window)
			{
				return;
			}

			int currentLogicalW = 0, currentLogicalH = 0;
			SDL_GetWindowSize(window, &currentLogicalW, &currentLogicalH);

			if (currentLogicalW <= 0 || currentLogicalH <= 0)
			{
				return;
			}

			float scaleX = static_cast<float>(currentLogicalW) / static_cast<float>(referenceWindowWidth);
			float scaleY = static_cast<float>(currentLogicalH) / static_cast<float>(referenceWindowHeight);

			std::map<std::string, IFrameScreenData> sourceFrameDataMap;
			{
				std::lock_guard<std::mutex> lock(ui_panel_mutex_);
				if ((panel_frames_overridden_by_splitters_ || splitter_mouse_mecanic_.isOperating()) && !ui_panel_geometry_frame_data_map_.empty())
				{
					sourceFrameDataMap = ui_panel_geometry_frame_data_map_;
				}
				else if (!panelFrameDataMap.empty())
				{
					sourceFrameDataMap = panelFrameDataMap;
				}
				else
				{
					sourceFrameDataMap = ui_panel_geometry_frame_data_map_;
				}
			}

			if (sourceFrameDataMap.empty())
			{
				return;
			}

			if (frameMapMatchesWindowSize(sourceFrameDataMap, currentLogicalW, currentLogicalH))
			{
				static constexpr int BORDER = 3;
				int borderLeft = BORDER;
				int borderTop = BORDER;
				int borderWidth = drawableWidth - 2 * BORDER;
				int borderHeight = drawableHeight - 2 * BORDER;

				panel_map_builder_.drawInsideAppBorders(
					commandBuffer, drawableWidth, drawableHeight,
					sourceFrameDataMap, skipTextureRebuild,
					vk_context_, vulkan_pipelines_, vk_render_pass_,
					ui_panels_, window,
					borderLeft, borderTop, borderWidth, borderHeight);
				return;
			}

			std::map<std::string, IFrameScreenData> scaledFrameDataMap;

			for (const auto& pair : sourceFrameDataMap)
			{
				IFrameScreenData scaled = pair.second;
				scaled.relativeX = static_cast<int>(std::round(pair.second.relativeX * scaleX));
				scaled.relativeY = static_cast<int>(std::round(pair.second.relativeY * scaleY));
				scaled.width = static_cast<int>(std::round(pair.second.width * scaleX));
				scaled.height = static_cast<int>(std::round(pair.second.height * scaleY));
				scaled.clientX = static_cast<int>(std::round(pair.second.clientX * scaleX));
				scaled.clientY = static_cast<int>(std::round(pair.second.clientY * scaleY));
				scaledFrameDataMap[pair.first] = scaled;
			}

			static constexpr int BORDER = 3;
			int borderLeft = BORDER;
			int borderTop = BORDER;
			int borderWidth = drawableWidth - 2 * BORDER;
			int borderHeight = drawableHeight - 2 * BORDER;

			panel_map_builder_.drawInsideAppBorders(
				commandBuffer, drawableWidth, drawableHeight,
				scaledFrameDataMap, skipTextureRebuild,
				vk_context_, vulkan_pipelines_, vk_render_pass_,
				ui_panels_, window,
				borderLeft, borderTop, borderWidth, borderHeight);
		}

		const WorkSpace& UIManager::getWorkSpace() const
		{
			return panel_map_builder_.getWorkSpace();
		}

		void UIManager::clearUIPanels()
		{
			std::lock_guard<std::mutex> lock(ui_panel_mutex_);
			panel_map_builder_.clearUIPanels(ui_panels_);
			ui_panel_iframe_map_.clear();
			ui_panel_frame_data_map_.clear();
			ui_panel_geometry_frame_data_map_.clear();
			ui_panels_initialized_ = false;
		}

		std::map<std::string, IFrameData> UIManager::getUIPanelIFrames() const
		{
			std::lock_guard<std::mutex> lock(ui_panel_mutex_);
			return ui_panel_iframe_map_;
		}

		std::map<std::string, IFrameData> UIManager::getResolvedUIPanelIFrames() const
		{
			std::lock_guard<std::mutex> lock(ui_panel_mutex_);

			const auto& effectiveFrameDataMap = ui_panel_geometry_frame_data_map_.empty()
				? ui_panel_frame_data_map_
				: ui_panel_geometry_frame_data_map_;

			// ------ Redraw Texture at Splitter action ------ //
			return panel_map_builder_.buildIFrameDataMap(effectiveFrameDataMap);
		}

		std::map<std::string, IFrameScreenData> UIManager::getUIPanelFrameDatas() const
		{
			std::lock_guard<std::mutex> lock(ui_panel_mutex_);
			return ui_panel_geometry_frame_data_map_;
		}
    }
}