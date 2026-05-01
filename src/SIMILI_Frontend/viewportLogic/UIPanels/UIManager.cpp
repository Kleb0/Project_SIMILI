#include "UIManager.hpp"
#include "SplitterMouseMecanic.hpp"
#include "../../ThreadSafeIFrameMap.hpp"
#include "../../../Engine/VulkanScene/VKcontext.hpp"
#include <iostream>
#include <algorithm>
#include <set>
#include <cmath>

namespace SIMILI {
	namespace Frontend {

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
			, ui_panels_initialized_(false)
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
				return;
			}

			if (!frameDatas)
			{
				return;
			}

			const auto& frameDataMap = frameDatas->getFrameData();
			std::lock_guard<std::mutex> lock(ui_panel_mutex_);
			ui_panel_frame_data_map_.clear();

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
				}
			}
		}

		void UIManager::syncFrameDatas(SIMILI::Frontend::FrameDatas* frameDatas, SDL_Window* sdlWindow)
		{
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
				ui_panel_frame_data_map_.clear();

				for (const auto& pair : frameDataMap)
				{
					if (pair.first == "viewport_panel")
					{
						continue;
					}

					ui_panel_frame_data_map_[pair.first] = pair.second;
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

			if (!ui_panel_frame_data_map_.empty())
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
			const float fw = static_cast<float>(cefWidth);
			const float fh = static_cast<float>(cefHeight);
			std::lock_guard<std::mutex> lock(ui_panel_mutex_);
			for (auto& pair : ui_panels_)
			{
				if (!pair.second)
					continue;
				float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
				auto it = ui_panel_frame_data_map_.find(pair.first);
				if (it != ui_panel_frame_data_map_.end())
				{
					u0 = static_cast<float>(it->second.relativeX) / fw;
					v0 = static_cast<float>(it->second.relativeY) / fh;
					u1 = static_cast<float>(it->second.relativeX + it->second.width) / fw;
					v1 = static_cast<float>(it->second.relativeY + it->second.height) / fh;
				}
				pair.second->setCEFTexture(view, sampler, u0, v0, u1, v1);
			}
		}

		void UIManager::drawUIPanelsInsideBorders(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, const std::map<std::string, IFrameScreenData>& panelFrameDataMap, bool skipTextureRebuild, SDL_Window* window)
		{
			std::map<std::string, IFrameScreenData> effectivePanelFrameDataMap;
			if (!panelFrameDataMap.empty())
			{
				effectivePanelFrameDataMap = panelFrameDataMap;
			}
			else
			{
				std::lock_guard<std::mutex> lock(ui_panel_mutex_);
				effectivePanelFrameDataMap = ui_panel_frame_data_map_;
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
				sd.x      = static_cast<int>(std::round(def.x      * scaleX));
				sd.y      = static_cast<int>(std::round(def.y      * scaleY));
				sd.width  = static_cast<int>(std::round(def.width  * scaleX));
				sd.height = static_cast<int>(std::round(def.height * scaleY));
				sd.isVertical = def.isVertical;
				sd.isHorizontal = def.isHorizontal;
				splitterData.push_back(sd);
			}

			splitter_renderer_->setSplitters(splitterData);
			splitter_renderer_->draw(commandBuffer, drawableWidth, drawableHeight);
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
			}
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

			const std::map<std::string, IFrameScreenData>* sourceMap = &panelFrameDataMap;
			std::map<std::string, IFrameScreenData> fallbackSource;

			if (panelFrameDataMap.empty())
			{
				std::lock_guard<std::mutex> lock(ui_panel_mutex_);
				fallbackSource = ui_panel_frame_data_map_;
				sourceMap = &fallbackSource;
			}

			std::map<std::string, IFrameScreenData> scaledFrameDataMap;

			for (const auto& pair : *sourceMap)
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

		void UIManager::clearUIPanels()
		{
			std::lock_guard<std::mutex> lock(ui_panel_mutex_);
			panel_map_builder_.clearUIPanels(ui_panels_);
			ui_panel_iframe_map_.clear();
			ui_panel_frame_data_map_.clear();
			ui_panels_initialized_ = false;
		}

		std::map<std::string, IFrameData> UIManager::getUIPanelIFrames() const
		{
			std::lock_guard<std::mutex> lock(ui_panel_mutex_);
			return ui_panel_iframe_map_;
		}

		std::map<std::string, IFrameScreenData> UIManager::getUIPanelFrameDatas() const
		{
			std::lock_guard<std::mutex> lock(ui_panel_mutex_);
			return ui_panel_frame_data_map_;
		}
    }
}