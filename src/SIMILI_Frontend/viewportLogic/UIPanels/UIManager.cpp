#include "UIManager.hpp"
#include "../../ThreadSafeIFrameMap.hpp"
#include "../../CEFDrawing/CEF_Drawer.hpp"
#include "../../../Engine/VulkanScene/VKcontext.hpp"
#include <iostream>
#include <algorithm>
#include <set>

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
			, cef_drawer_(nullptr)
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
		}

		void UIManager::drawUIPanels(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, const std::map<std::string, IFrameScreenData>& panelFrameDataMap, bool skipTextureRebuild, SDL_Window* window)
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

		panel_map_builder_.drawUIPanels(
			commandBuffer, 
			drawableWidth, 
			drawableHeight, 
			effectivePanelFrameDataMap, 
			skipTextureRebuild,
			vk_context_,
			vulkan_pipelines_,
			vk_render_pass_,
			cef_drawer_,
			ui_panels_,
			window
		);
	}

		void UIManager::RenderUI(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, const std::map<std::string, IFrameScreenData>& panelFrameDataMap, bool skipTextureRebuild)
		{
			std::cout << "[UIManager] RenderUI called with " << panelFrameDataMap.size() << " panels" << std::endl;
			drawUIPanels(commandBuffer, drawableWidth, drawableHeight, panelFrameDataMap, skipTextureRebuild, nullptr);
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