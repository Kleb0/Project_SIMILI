#pragma once

#include "../FrameDatas/FrameDatas.hpp"
#include <SDL3/SDL.h>
#include <map>
#include <string>
#include <vector>

class VKContext;

namespace SIMILI {
	namespace Frontend {
		
		struct PanelState
		{
			IFrameScreenData frame;
			int client_offset_x;
			int client_offset_y;
		};

		struct SplitterDefinition
		{
			int x;
			int y;
			int width;
			int height;
			bool isVertical;
		};

		class PanelMapBuilder
		{
		public:
			PanelMapBuilder();
			~PanelMapBuilder();

			void syncSplittersAndPanes(
				const std::map<std::string, IFrameScreenData>& frameDataMap,
				int currentWindowWidth,
				int currentWindowHeight,
				std::map<std::string, PanelState>& panelStateMap,
				std::vector<SplitterDefinition>& splitterList,
				int& lastWindowWidth,
				int& lastWindowHeight,
				SDL_Window* window = nullptr
			);

			void rebuildFromSource(
				const std::map<std::string, IFrameScreenData>& frameDataMap,
				int currentWindowWidth,
				int currentWindowHeight,
				std::map<std::string, PanelState>& panelStateMap,
				int& lastWindowWidth,
				int& lastWindowHeight
			);

			void scaleLayoutToWindow(
				int newWindowWidth,
				int newWindowHeight,
				std::map<std::string, PanelState>& panelStateMap,
				int& lastWindowWidth,
				int& lastWindowHeight,
				SDL_Window* window
			);

			void updateClientCoordinates(std::map<std::string, PanelState>& panelStateMap);

			void drawUIPanels(
				VkCommandBuffer commandBuffer,int drawableWidth,
				int drawableHeight, const std::map<std::string, IFrameScreenData>& panelFrameDataMap,
				bool skipTextureRebuild, 
				class VKContext* vkContext, class VulkanPipeline* vulkanPipelines, VkRenderPass renderPass,
				class CEF_Drawer* cefDrawer,
				std::map<std::string, std::unique_ptr<class UIPanel>>& uiPanels,
				SDL_Window* window);

			void clearUIPanels(std::map<std::string, std::unique_ptr<class UIPanel>>& uiPanels);

	private:
		void createViewportPanel(
			std::map<std::string, PanelState>& panelStateMap,
			int currentWindowWidth,
			int currentWindowHeight
		);
			void buildSplitters(
				const std::map<std::string, PanelState>& panelStateMap,
				std::vector<SplitterDefinition>& splitterList
			);
		};
	}
}
