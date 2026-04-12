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
			clearUIPanels();
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

		void UIManager::drawUIPanels(VkCommandBuffer commandBuffer, int drawableWidth, int drawableHeight, const std::map<std::string, IFrameScreenData>& panelFrameDataMap, bool skipTextureRebuild)
		{
			if (!cef_drawer_)
			{
				return;
			}

			SDL_Window* sdlWindow = cef_drawer_->getWindowHandle();
			if (!sdlWindow)
			{
				return;
			}

			if (panelFrameDataMap.empty())
			{
				if (!ui_panels_initialized_)
				{
					std::cout << "[UIManager] drawUIPanels: No panel data available - clearing panels" << std::endl;
				}
				ui_panels_.clear();
				return;
			}
			
			static int panel_map_log_count = 0;
			if (panel_map_log_count < 5 || panel_map_log_count % 120 == 0)
			{
				std::cout << "[UIManager::drawUIPanels] Rendering " << panelFrameDataMap.size() << " panels at call " << panel_map_log_count << ":" << std::endl;
				for (const auto& p : panelFrameDataMap)
				{
					std::cout << "  - " << p.first << " at (" << p.second.relativeX << "," << p.second.relativeY << ") size " << p.second.width << "x" << p.second.height << std::endl;
				}
			}
			panel_map_log_count++;

			CEF_Drawer::SDLWindowProperties windowProperties = cef_drawer_->getSDLWindowProperties();
			if (drawableWidth <= 0 || drawableHeight <= 0)
			{
				drawableWidth = windowProperties.drawable_width;
				drawableHeight = windowProperties.drawable_height;
			}
			if (drawableWidth <= 0 || drawableHeight <= 0)
			{
				SDL_GetWindowSizeInPixels(sdlWindow, &drawableWidth, &drawableHeight);
			}
			if (drawableWidth <= 0 || drawableHeight <= 0)
			{
				return;
			}

			std::set<std::string> activePanelNames;

			for (const auto& pair : panelFrameDataMap)
			{
				if (pair.first == "viewport_panel")
				{
					if (!ui_panels_initialized_)
					{
						std::cout << "[UIManager] Skipping viewport_panel from UI rendering" << std::endl;
					}
					continue;
				}

				if (pair.second.width <= 0 || pair.second.height <= 0)
				{
					if (!ui_panels_initialized_)
					{
						std::cout << "[UIManager] Skipping panel '" << pair.first << "' with invalid dimensions: " << pair.second.width << "x" << pair.second.height << std::endl;
					}
					continue;
				}
				
				auto panelIt = ui_panels_.find(pair.first);
				if (panelIt == ui_panels_.end())
				{
					std::cout << "[UIManager] Creating new UIPanel: " << pair.first << std::endl;
					auto panel = std::make_unique<UIPanel>();
					if (!panel->initialize(pair.first))
					{
						std::cout << "[UIManager] Failed to initialize panel: " << pair.first << std::endl;
						continue;
					}

					if (vk_context_)
					{
						panel->setVKContext(vk_context_);
						std::cout << "[UIManager] VKContext set for panel: " << pair.first << std::endl;
					}
					else
					{
						std::cout << "[UIManager] No VKContext available for panel: " << pair.first << std::endl;
					}

					if (vulkan_pipelines_)
					{
						panel->setVulkanPipelines(vulkan_pipelines_);
						std::cout << "[UIManager] VulkanPipelines set for panel: " << pair.first << std::endl;
					}
					else
					{
						std::cout << "[UIManager] No VulkanPipelines available for panel: " << pair.first << std::endl;
					}

					if (vk_render_pass_ != VK_NULL_HANDLE)
					{
						panel->setRenderPass(vk_render_pass_);
						std::cout << "[UIManager] RenderPass set for panel: " << pair.first << std::endl;
					}
					else
					{
						std::cout << "[UIManager] RenderPass is NULL for panel: " << pair.first << std::endl;
					}

					panelIt = ui_panels_.emplace(pair.first, std::move(panel)).first;
					std::cout << "[UIManager] Panel created and stored: " << pair.first << std::endl;
				}

				panelIt->second->updateFromFrameData(pair.second, sdlWindow, skipTextureRebuild);
				
				UIPanel::DrawingState preDrawState = panelIt->second->getDrawingState();
				
				panelIt->second->draw(commandBuffer, drawableWidth, drawableHeight);
				
				UIPanel::DrawingState postDrawState = panelIt->second->getDrawingState();
				
				static int draw_debug_count = 0;
				bool should_log = (draw_debug_count < 3 || draw_debug_count % 120 == 0);
				if (should_log || preDrawState == UIPanel::DrawingState::IsNotReadyToBeDrawn)
				{
					std::cout << "[UIManager::drawUIPanels] Panel " << pair.first 
							  << " state: " << static_cast<int>(preDrawState) << "->" << static_cast<int>(postDrawState)
							  << " at draw call " << draw_debug_count << std::endl;
				}
				draw_debug_count++;
				
				activePanelNames.insert(pair.first);
			}

			for (auto it = ui_panels_.begin(); it != ui_panels_.end();)
			{
				if (activePanelNames.find(it->first) == activePanelNames.end())
				{
					it = ui_panels_.erase(it);
				}
				else
				{
					++it;
				}
			}

			if (!ui_panels_initialized_ && !ui_panels_.empty())
			{
				ui_panels_initialized_ = true;
				std::cout << "[UIManager] UI Panels initialization completed with " << ui_panels_.size() << " panels" << std::endl;
			}
		}

		void UIManager::clearUIPanels()
		{
			std::lock_guard<std::mutex> lock(ui_panel_mutex_);
			ui_panels_.clear();
			ui_panel_frame_data_map_.clear();
			ui_panel_iframe_map_.clear();
			ui_panels_initialized_ = false;
		}

		void UIManager::forceRebuildAllUIPanels()
		{
			std::lock_guard<std::mutex> lock(ui_panel_mutex_);
			std::cout << "[UIManager] Forcing rebuild of all UI panels..." << std::endl;
			
			for (auto& pair : ui_panels_)
			{
				if (pair.second)
				{
					pair.second->forceTextureRebuild();
					std::cout << "[UIManager] Forced rebuild for panel: " << pair.first << std::endl;
				}
			}
			
			ui_panels_initialized_ = false;
			std::cout << "[UIManager] All UI panels rebuild forced" << std::endl;
		}

		void UIManager::forceRedrawAllUIPanels()
		{
			std::lock_guard<std::mutex> lock(ui_panel_mutex_);
			for (auto& pair : ui_panels_)
			{
				if (pair.second)
				{
					pair.second->forceRedraw();
				}
			}
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