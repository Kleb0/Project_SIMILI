#pragma once

#include "../FrameDatas/FrameDatas.hpp"
#include "../../ThreadSafeIFrameMap.hpp"
#include "WorkSpace.hpp"
#include <SDL3/SDL.h>
#include <map>
#include <string>
#include <vector>
#include <climits>

class VKContext;
enum WindowRenderState : int;

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
			bool isHorizontal;
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
			RayHitType hitType;
			std::string hitPanelName;
		};

		struct SplitterCandidate
		{
			int x;
			int y;
			int width;
			int height;
			bool isVertical;
			bool isHorizontal;
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

		enum class SplitterBoundaryRole
		{
			None,
			WorkspaceLeft,
			WorkspaceRight,
			WorkspaceTop,
			WorkspaceBottom
		};

		struct MapData
		{
			std::map<std::string, PanelMapEntry>  panels;
			std::vector<SplitterDefinition> splitters;
			std::vector<SplitterCandidate> splitterCandidates;
			std::vector<std::vector<int>> splitterAttachments;
			std::vector<SplitterBoundaryRole> splitterBoundaryRoles;

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
					std::vector<SplitterDefinition>& splitterList,
					int currentWindowWidth);

				const MapData& getMapData() const;

				std::map<std::string, IFrameData> buildIFrameDataMap(
					const std::map<std::string, IFrameScreenData>& frameDataMap) const;

				void attachedUpdatedMap(
					const std::map<std::string, IFrameScreenData>& frameDataMap,
					const std::vector<SplitterDefinition>& splitterList,
					WindowRenderState currentWindowState);

				void AttachedSplittersWhenMoving(std::vector<SplitterDefinition>& splitters);

				void ClampBoundarySplitters(std::vector<SplitterDefinition>& splitters, int windowWidth, int windowHeight);

				const WorkSpace& getWorkSpace() const;

				void updateWorkSpaceFromSplitters(const std::vector<SplitterDefinition>& splitterList, int windowWidth, int windowHeight);

			private:
				bool frameMapMatchesWindowSize(
					const std::map<std::string, IFrameScreenData>& frameDataMap,
					int windowWidth,
					int windowHeight) const;

				void syncPanelStateMapFromFrameData(
					const std::map<std::string, IFrameScreenData>& frameDataMap,
					std::map<std::string, PanelState>& panelStateMap);

				std::map<std::string, IFrameScreenData> buildFrameDataMapFromPanelState(
				const std::map<std::string, PanelState>& panelStateMap,
				const std::map<std::string, IFrameScreenData>& fallbackFrameDataMap) const;

				void attachedSplittersAtCreation();

				void createViewportPanel(std::map<std::string, PanelState>& panelStateMap,
				int currentWindowWidth, int currentWindowHeight);

				//void buildSplitters(const std::map<std::string, PanelState>& panelStateMap,
				//	std::vector<SplitterDefinition>& splitterList);

				void buildSplittersFromRays(
					const std::map<std::string, IFrameScreenData>& frameDataMap);

				void buildWorkSpace(
					const std::map<std::string, IFrameScreenData>& frameDataMap);

				RayCastResult castRay(
					const std::string& sourceName,
					const IFrameScreenData& source,
					RayDirection direction,
					const std::map<std::string, IFrameScreenData>& frameDataMap,
					int borderLeft, int borderTop, int borderRight, int borderBottom) const;

				MapData map_data_;
				WorkSpace workspace_;
		};
	}
}
