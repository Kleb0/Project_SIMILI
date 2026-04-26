#pragma once

#include "../FrameDatas/FrameDatas.hpp"
#include <SDL3/SDL.h>
#include <map>
#include <string>
#include <vector>
#include <climits>

class VKContext;

namespace SIMILI 
{
	namespace Frontend 
	{
		
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

		enum class PanelLayoutType
		{
			Row,
			Column
		};

		enum class RayDirection
		{
			Up,
			Down,
			Left,
			Right
		};

		enum class RayHitType
		{
			AppBorder,
			Panel,
			Nothing
		};

		struct RayCastResult
		{
			RayDirection direction;
			RayHitType   hitType;
			std::string  hitPanelName;
		};

		struct SplitterCandidate
		{
			int x;
			int y;
			int width;
			int height;
			bool isVertical;
			std::vector<std::string> assignedPanels;
		};

		struct PanelMapEntry
		{
			std::string name;
			int index;
			PanelLayoutType layoutType;
			int column;
			int row;
		};

		struct MapData
		{
			std::map<std::string, PanelMapEntry>  panels;
			std::vector<SplitterDefinition>       splitters;
			std::vector<SplitterCandidate>        splitterCandidates;

			void clear();
		};

		class PanelMapBuilder
		{
			public:
				PanelMapBuilder();
				~PanelMapBuilder();

				void syncSplittersAndPanels(const std::map<std::string, IFrameScreenData>& frameDataMap,
					int currentWindowWidth,int currentWindowHeight,
					std::map<std::string, PanelState>& panelStateMap,
					std::vector<SplitterDefinition>& splitterList,
					int& lastWindowWidth,int& lastWindowHeight,
					SDL_Window* window = nullptr);

				//void rebuildFromSource(
				//	const std::map<std::string, IFrameScreenData>& frameDataMap,
				//	int currentWindowWidth,int currentWindowHeight,
				//	std::map<std::string, PanelState>& panelStateMap,
				//	int& lastWindowWidth, int& lastWindowHeight	);

				void scaleLayoutToWindow(
					int newWindowWidth,int newWindowHeight,
					std::map<std::string, PanelState>& panelStateMap,
					int& lastWindowWidth, int& lastWindowHeight,
					SDL_Window* window );

				void updateClientCoordinates(std::map<std::string, PanelState>& panelStateMap);

				void drawInsideAppBorders( VkCommandBuffer commandBuffer, int drawableWidth,
					int drawableHeight, const std::map<std::string, IFrameScreenData>& panelFrameDataMap,
					bool skipTextureRebuild,
					class VKContext* vkContext, class VulkanPipeline* vulkanPipelines, VkRenderPass renderPass,
					std::map<std::string, std::unique_ptr<class UIPanel>>& uiPanels,
					SDL_Window* window,
					int appBorderLeft, int appBorderTop,
					int appBorderWidth, int appBorderHeight);

				void clearUIPanels(std::map<std::string, std::unique_ptr<class UIPanel>>& uiPanels);

				void buildMapData(
					const std::map<std::string, IFrameScreenData>& frameDataMap,
					const std::vector<SplitterDefinition>& splitterList,
					int currentWindowWidth);

				const MapData& getMapData() const;

			private:

				void createViewportPanel(std::map<std::string, PanelState>& panelStateMap,
				int currentWindowWidth, int currentWindowHeight);

				//void buildSplitters(const std::map<std::string, PanelState>& panelStateMap,
				//	std::vector<SplitterDefinition>& splitterList);

				void buildSplittersFromRays(
					const std::map<std::string, IFrameScreenData>& frameDataMap);

				RayCastResult castRay(
					const std::string& sourceName,
					const IFrameScreenData& source,
					RayDirection direction,
					const std::map<std::string, IFrameScreenData>& frameDataMap,
					int borderLeft, int borderTop, int borderRight, int borderBottom) const;

				MapData map_data_;
		};
	}
}
