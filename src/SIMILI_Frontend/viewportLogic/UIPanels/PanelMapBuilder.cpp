#include "PanelMapBuilder.hpp"
#include "UIPanel.hpp"
#include "../../../Engine/VulkanScene/VKcontext.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>

namespace SIMILI {
	namespace Frontend {

		PanelMapBuilder::PanelMapBuilder()
		{
		}

		PanelMapBuilder::~PanelMapBuilder()
		{
		}

		void PanelMapBuilder::scaleLayoutToWindow(
		int newWindowWidth,int newWindowHeight,
		std::map<std::string, PanelState>& panelStateMap,
		int& lastWindowWidth, int& lastWindowHeight,SDL_Window* window)
		{
			if (newWindowWidth <= 0 || newWindowHeight <= 0)
			{
				return;
			}

			if (lastWindowWidth <= 0 || lastWindowHeight <= 0)
			{
				lastWindowWidth = newWindowWidth;
				lastWindowHeight = newWindowHeight;
				return;
			}

			const float scaleX = static_cast<float>(newWindowWidth) / static_cast<float>(lastWindowWidth);
			const float scaleY = static_cast<float>(newWindowHeight) / static_cast<float>(lastWindowHeight);
			if (scaleX <= 0.0f || scaleY <= 0.0f)
			{
				return;
			}
			
			std::cout << "[PanelMapBuilder::scaleLayoutToWindow] Scaling from " << lastWindowWidth << "x" << lastWindowHeight 
			          << " to " << newWindowWidth << "x" << newWindowHeight 
			          << " (scaleX=" << scaleX << ", scaleY=" << scaleY << ")" << std::endl;

			float dpiScale = 1.0f;
			if (window)
			{
				int drawableWidth = 0;
				int drawableHeight = 0;
				SDL_GetWindowSizeInPixels(window, &drawableWidth, &drawableHeight);
				if (newWindowWidth > 0 && drawableWidth > 0)
				{
					dpiScale = static_cast<float>(drawableWidth) / static_cast<float>(newWindowWidth);
				}
			}

			for (auto& pair : panelStateMap)
			{
				IFrameScreenData& frame = pair.second.frame;
				
				int oldX = frame.relativeX;
				int oldY = frame.relativeY;
				int oldW = frame.width;
				int oldH = frame.height;
				
				frame.relativeX = static_cast<int>(std::lround(static_cast<float>(frame.relativeX) * scaleX));
				frame.relativeY = static_cast<int>(std::lround(static_cast<float>(frame.relativeY) * scaleY));
				frame.width = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(frame.width) * scaleX)));
				frame.height = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(frame.height) * scaleY)));
				
				if (frame.relativeX >= newWindowWidth)
				{
					frame.relativeX = (std::max)(0, newWindowWidth - frame.width);
					std::cout << "[PanelMapBuilder::scaleLayoutToWindow] CLAMPED " << pair.first 
					          << " - was off-screen horizontally, moved to x=" << frame.relativeX << std::endl;
				}
				
				if (frame.relativeY >= newWindowHeight)
				{
					frame.relativeY = (std::max)(0, newWindowHeight - frame.height);
					std::cout << "[PanelMapBuilder::scaleLayoutToWindow] CLAMPED " << pair.first 
					          << " - was off-screen vertically, moved to y=" << frame.relativeY << std::endl;
				}
				
				if (frame.relativeX + frame.width > newWindowWidth)
				{
					frame.width = (std::max)(1, newWindowWidth - frame.relativeX);
					std::cout << "[PanelMapBuilder::scaleLayoutToWindow] CLAMPED " << pair.first 
					          << " width to " << frame.width << std::endl;
				}
				
				if (frame.relativeY + frame.height > newWindowHeight)
				{
					frame.height = (std::max)(1, newWindowHeight - frame.relativeY);
					std::cout << "[PanelMapBuilder::scaleLayoutToWindow] CLAMPED " << pair.first 
					          << " height to " << frame.height << std::endl;
				}
				
				frame.windowWidth = newWindowWidth;
				frame.windowHeight = newWindowHeight;
				frame.dpiScale = dpiScale;
				
				std::cout << "[PanelMapBuilder::scaleLayoutToWindow] Scaled " << pair.first 
				          << ": (" << oldX << "," << oldY << " " << oldW << "x" << oldH << ")"
				          << " -> (" << frame.relativeX << "," << frame.relativeY << " " 
				          << frame.width << "x" << frame.height << ")" << std::endl;
			}

			lastWindowWidth = newWindowWidth;
			lastWindowHeight = newWindowHeight;
		}

		void PanelMapBuilder::updateClientCoordinates(std::map<std::string, PanelState>& panelStateMap)
		{
			for (auto& pair : panelStateMap)
			{
				pair.second.frame.clientX = pair.second.frame.relativeX + pair.second.client_offset_x;
				pair.second.frame.clientY = pair.second.frame.relativeY + pair.second.client_offset_y;
			}
		}

		void PanelMapBuilder::syncSplittersAndPanels(
		const std::map<std::string, IFrameScreenData>& frameDataMap,
		int currentWindowWidth, int currentWindowHeight,
		std::map<std::string, PanelState>& panelStateMap,
		std::vector<SplitterDefinition>& splitterList,
		int& lastWindowWidth, int& lastWindowHeight,
		SDL_Window* window)
		{
			std::cout << "[PanelMapBuilder::syncSplittersAndPanes] Called with " << frameDataMap.size() 
			          << " panels for window " << currentWindowWidth << "x" << currentWindowHeight << std::endl;


			if (currentWindowWidth > 0 && currentWindowHeight > 0 && window)
			{
				int windowWidth = 0;
				int windowHeight = 0;
				SDL_GetWindowSize(window, &windowWidth, &windowHeight);
				
				if (windowWidth > 0 && windowHeight > 0 && 
				    (windowWidth != lastWindowWidth || windowHeight != lastWindowHeight))
				{
					std::cout << "[PanelMapBuilder::syncSplittersAndPanes] Window size changed, scaling..." << std::endl;
					scaleLayoutToWindow(windowWidth, windowHeight, panelStateMap, lastWindowWidth, lastWindowHeight, window);
				}
			}

			updateClientCoordinates(panelStateMap);

			buildMapData(frameDataMap, splitterList, currentWindowWidth);

			std::cout << "[PanelMapBuilder::syncSplittersAndPanes] Completed - " << frameDataMap.size() 
			          << " panels, " << splitterList.size() << " splitters" << std::endl;
		}

		void PanelMapBuilder::createViewportPanel(
		std::map<std::string, PanelState>& panelStateMap,
		int currentWindowWidth, int currentWindowHeight)
		{
			std::cout << "[PanelMapBuilder::createViewportPanel] Checking viewport_panel auto-creation: window=" 
			          << currentWindowWidth << "x" << currentWindowHeight 
			          << " panels=" << panelStateMap.size() << std::endl;
			
			if (currentWindowWidth > 0 && currentWindowHeight > 0 && !panelStateMap.empty())
			{
				std::cout << "[PanelMapBuilder::createViewportPanel] Conditions met - calculating viewport bounds..." << std::endl;
				
				int leftPanelRight = 0;
				int rightPanelLeft = currentWindowWidth;
				int topPanelBottom = 0;
				int bottomPanelTop = currentWindowHeight;
				
				for (const auto& pair : panelStateMap)
				{
					const auto& frame = pair.second.frame;
					if (frame.width > 0 && frame.height > 0)
					{
						if (frame.relativeX < (currentWindowWidth / 4))
						{
							leftPanelRight = (std::max)(leftPanelRight, frame.relativeX + frame.width);
						}
						
						if (frame.relativeX > (currentWindowWidth * 3 / 4))
						{
							rightPanelLeft = (std::min)(rightPanelLeft, frame.relativeX);
						}
					}
				}
				
				const int MIN_SPLITTER_SPACE = 3;
				const int viewportLeft = leftPanelRight + MIN_SPLITTER_SPACE;
				const int viewportRight = rightPanelLeft - MIN_SPLITTER_SPACE;
				const int viewportWidth = viewportRight - viewportLeft;
				
				std::cout << "[PanelMapBuilder::createViewportPanel] Calculated viewport width: left=" << viewportLeft 
				          << " right=" << viewportRight << " width=" << viewportWidth << std::endl;
				
				if (viewportWidth > 50 && currentWindowHeight > 100)
				{
					for (const auto& pair : panelStateMap)
					{
						const auto& frame = pair.second.frame;
						if (frame.width > 0 && frame.height > 0)
						{
							topPanelBottom = (std::max)(topPanelBottom, frame.relativeY + frame.height);
							if (frame.relativeY + frame.height > (currentWindowHeight / 2))
							{
								bottomPanelTop = (std::min)(bottomPanelTop, frame.relativeY);
							}
						}
					}
					
					const int viewportTop = 3;
					const int viewportBottom = (bottomPanelTop > topPanelBottom) ? bottomPanelTop - MIN_SPLITTER_SPACE : topPanelBottom - MIN_SPLITTER_SPACE;
					const int viewportHeight = viewportBottom - viewportTop;
					
					std::cout << "[PanelMapBuilder::createViewportPanel] Calculated viewport height: top=" << viewportTop 
					          << " bottom=" << viewportBottom << " height=" << viewportHeight 
					          << " (topPanelBottom=" << topPanelBottom << " bottomPanelTop=" << bottomPanelTop << ")" << std::endl;
					
					if (viewportHeight > 50)
					{
						PanelState viewportState;
						viewportState.frame.name = "viewport_panel";
						viewportState.frame.relativeX = viewportLeft;
						viewportState.frame.relativeY = viewportTop;
						viewportState.frame.clientX = viewportLeft;
						viewportState.frame.clientY = viewportTop;
						viewportState.frame.width = viewportWidth;
						viewportState.frame.height = viewportHeight;
						viewportState.frame.windowWidth = currentWindowWidth;
						viewportState.frame.windowHeight = currentWindowHeight;
						viewportState.frame.screenX = 0;
						viewportState.frame.screenY = 0;
						viewportState.frame.dpiScale = 1.0f;
						viewportState.client_offset_x = 0;
						viewportState.client_offset_y = 0;
						
						panelStateMap["viewport_panel"] = viewportState;
						
						std::cout << "[PanelMapBuilder::createViewportPanel] AUTO-CREATED viewport_panel at (" 
						          << viewportLeft << "," << viewportTop << ") size " 
						          << viewportWidth << "x" << viewportHeight << std::endl;
					}
				}
			}
		}

		void PanelMapBuilder::drawInsideAppBorders(
		VkCommandBuffer commandBuffer,
		int drawableWidth, int drawableHeight,
		const std::map<std::string, IFrameScreenData>& panelFrameDataMap,
		bool skipTextureRebuild, 
		VKContext* vkContext, VulkanPipeline* vulkanPipelines, VkRenderPass renderPass,
		std::map<std::string, std::unique_ptr<UIPanel>>& uiPanels,
		SDL_Window* window,
		int appBorderLeft, int appBorderTop, int appBorderWidth, int appBorderHeight)
		{
	
			if (panelFrameDataMap.empty())
			{
				std::cout << "[PanelMapBuilder] drawInsideAppBorders: No panel data available" << std::endl;
				return;
			}

			// Viewport offset = App_Border origin: NDC(-1,-1) maps to (borderLeft, borderTop) on screen.
			// This translates the entire group of panels by the border offset, without any clipping.

			VkViewport viewport = {};
			viewport.x = static_cast<float>(appBorderLeft);
			viewport.y = static_cast<float>(appBorderTop);
			viewport.width  = static_cast<float>(appBorderWidth);
			viewport.height = static_cast<float>(appBorderHeight);
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

			// Full-framebuffer scissor: no clipping.
			VkRect2D scissor = {};
			scissor.offset = {0, 0};
			scissor.extent = {static_cast<uint32_t>(drawableWidth), static_cast<uint32_t>(drawableHeight)};
			vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

			for (const auto& pair : panelFrameDataMap)
			{
				const std::string& panelName = pair.first;
				const auto& frameData = pair.second;

				if (panelName == "viewport_panel")
				{
					continue;
				}

				auto it = uiPanels.find(panelName);

				if (it == uiPanels.end())
				{
					auto newPanel = std::make_unique<UIPanel>();
					if (newPanel->initialize(panelName))
					{
						newPanel->setVKContext(vkContext);
						newPanel->setVulkanPipelines(vulkanPipelines);
						newPanel->setRenderPass(renderPass);

						uiPanels[panelName] = std::move(newPanel);
						it = uiPanels.find(panelName);
					}

				}

				if (it != uiPanels.end() && it->second)
				{
					it->second->updateFromFrameData(frameData, window, skipTextureRebuild);
					it->second->draw(commandBuffer, appBorderWidth, appBorderHeight);
				}
			}
		}

		void PanelMapBuilder::clearUIPanels(std::map<std::string, std::unique_ptr<UIPanel>>& uiPanels)
		{
			for (auto& pair : uiPanels)
			{
				if (pair.second)
				{
					pair.second->shutdown();
				}
			}
			uiPanels.clear();
			std::cout << "[PanelMapBuilder] clearUIPanels: All UI panels cleared" << std::endl;
		}

		void MapData::clear()
		{
			panels.clear();
			splitters.clear();
			splitterCandidates.clear();
		}

		void PanelMapBuilder::buildMapData(
			const std::map<std::string, IFrameScreenData>& frameDataMap,
			std::vector<SplitterDefinition>& splitterList,
			int currentWindowWidth)
		{
			map_data_.clear();

			map_data_.splitters = splitterList;

			if (frameDataMap.empty() || currentWindowWidth <= 0)
			{
				return;
			}

			const float fullWidthThreshold = 0.85f;

			struct SortEntry
			{
				std::string key;
				const IFrameScreenData* data;
			};

			std::vector<SortEntry> rowPanels;
			std::vector<SortEntry> columnPanels;

			for (const auto& pair : frameDataMap)
			{
				if (pair.first == "viewport_panel")
				{
					continue;
				}

				const IFrameScreenData& fd = pair.second;
				float widthRatio = static_cast<float>(fd.width) / static_cast<float>(currentWindowWidth);

				SortEntry entry;
				entry.key  = pair.first;
				entry.data = &fd;

				if (widthRatio >= fullWidthThreshold)
				{
					rowPanels.push_back(entry);
				}
				else
				{
					columnPanels.push_back(entry);
				}
			}

			std::sort(rowPanels.begin(), rowPanels.end(),
				[](const SortEntry& a, const SortEntry& b)
				{
					return a.data->relativeY < b.data->relativeY;
				});

			std::sort(columnPanels.begin(), columnPanels.end(),
				[](const SortEntry& a, const SortEntry& b)
				{
					if (a.data->relativeX != b.data->relativeX)
					{
						return a.data->relativeX < b.data->relativeX;
					}
					return a.data->relativeY < b.data->relativeY;
				});

			int nextIndex = 1;

			for (const auto& entry : rowPanels)
			{
				PanelMapEntry mapEntry;
				mapEntry.name       = entry.key;
				mapEntry.index      = nextIndex;
				mapEntry.layoutType = PanelLayoutType::Row;
				mapEntry.column     = 0;
				mapEntry.row        = nextIndex;
				map_data_.panels[entry.key] = mapEntry;
				++nextIndex;
			}

			int currentColumn = 0;
			int prevX         = -1;
			int rowInColumn   = 0;

			for (const auto& entry : columnPanels)
			{
				if (entry.data->relativeX != prevX)
				{
					++currentColumn;
					rowInColumn = 1;
					prevX = entry.data->relativeX;
				}
				else
				{
					++rowInColumn;
				}

				PanelMapEntry mapEntry;
				mapEntry.name       = entry.key;
				mapEntry.index      = nextIndex;
				mapEntry.layoutType = PanelLayoutType::Column;
				mapEntry.column     = currentColumn;
				mapEntry.row        = rowInColumn;
				map_data_.panels[entry.key] = mapEntry;
				++nextIndex;
			}

			std::cout << "\n -------------- [Panel Map Building ] --------------" << std::endl;
			std::cout << "[PanelMapBuilder::buildMapData] Layout map:" << std::endl;

			std::vector<const PanelMapEntry*> ordered;

			for (const auto& pair : map_data_.panels)
			{
				ordered.push_back(&pair.second);
			}

			std::sort(ordered.begin(), ordered.end(),
				[](const PanelMapEntry* a, const PanelMapEntry* b)
				{
					return a->index < b->index;
				});

			for (const PanelMapEntry* e : ordered)
			{
				std::cout << "  " << e->name << " = " << e->index << std::endl;
			}

			buildSplittersFromRays(frameDataMap);

			splitterList.clear();
			for (const auto& sc : map_data_.splitterCandidates)
			{
				SplitterDefinition def;
				def.x = sc.x;
				def.y = sc.y;
				def.width = sc.width;
				def.height = sc.height;
				def.isVertical = sc.isVertical;
				splitterList.push_back(def);
			}

			std::cout << "[PanelMapBuilder::buildMapData] "
			          << map_data_.panels.size() << " panels, "
			          << map_data_.splitterCandidates.size() << " splitters" << std::endl;

			int splitterIdx = 1;
			for (const auto& sc : map_data_.splitterCandidates)
			{
				std::cout << "  Splitter " << splitterIdx++ << " ("
				          << (sc.isVertical ? "V" : "H") << ") at ("
				          << sc.x << "," << sc.y << ") "
				          << sc.width << "x" << sc.height << " -> panels: ";
				for (std::size_t i = 0; i < sc.assignedPanels.size(); ++i)
				{
					if (i > 0) std::cout << ", ";
					std::cout << sc.assignedPanels[i];
				}
				std::cout << std::endl;
			}

			std::cout << " ------------------ [End of Panel Map Building ] -----------------------------\n" << std::endl;
		}

		const MapData& PanelMapBuilder::getMapData() const
		{
			return map_data_;
		}

		RayCastResult PanelMapBuilder::castRay(
			const std::string& sourceName,
			const IFrameScreenData& source,
			RayDirection direction,
			const std::map<std::string, IFrameScreenData>& frameDataMap,
			int borderLeft, int borderTop, int borderRight, int borderBottom) const
		{
			RayCastResult result;
			result.direction = direction;
			result.hitType = RayHitType::Nothing;

			const int TOLERANCE = 6;

			int px = source.relativeX;
			int py = source.relativeY;
			int pRight = px + source.width;
			int pBottom = py + source.height;

			if (direction == RayDirection::Left && px <= borderLeft + TOLERANCE)
			{
				result.hitType = RayHitType::AppBorder;
				return result;
			}
			if (direction == RayDirection::Up && py <= borderTop + TOLERANCE)
			{
				result.hitType = RayHitType::AppBorder;
				return result;
			}
			if (direction == RayDirection::Right && pRight >= borderRight - TOLERANCE)
			{
				result.hitType = RayHitType::AppBorder;
				return result;
			}
			if (direction == RayDirection::Down && pBottom >= borderBottom - TOLERANCE)
			{
				result.hitType = RayHitType::AppBorder;
				return result;
			}

			std::string neighborName;
			int bestDist = INT_MAX;

			for (const auto& otherPair : frameDataMap)
			{
				if (otherPair.first == sourceName) continue;
				if (otherPair.first == "viewport_panel") continue;

				const IFrameScreenData& other = otherPair.second;
				int ox = other.relativeX;
				int oy = other.relativeY;
				int oRight = ox + other.width;
				int oBottom = oy + other.height;

				int dist = INT_MAX;
				bool valid = false;

				if (direction == RayDirection::Right)
				{
					if (ox >= pRight - TOLERANCE && ox <= pRight + 60)
					{
						if (oBottom > py && oy < pBottom)
						{
							dist  = ox - pRight;
							valid = true;
						}
					}
				}
				else if (direction == RayDirection::Left)
				{
					if (oRight <= px + TOLERANCE && oRight >= px - 60)
					{
						if (oBottom > py && oy < pBottom)
						{
							dist  = px - oRight;
							valid = true;
						}
					}
				}
				else if (direction == RayDirection::Down)
				{
					if (oy >= pBottom - TOLERANCE && oy <= pBottom + 60)
					{
						if (oRight > px && ox < pRight)
						{
							dist  = oy - pBottom;
							valid = true;
						}
					}
				}
				else if (direction == RayDirection::Up)
				{
					if (oBottom <= py + TOLERANCE && oBottom >= py - 60)
					{
						if (oRight > px && ox < pRight)
						{
							dist  = py - oBottom;
							valid = true;
						}
					}
				}

				if (valid && dist < bestDist)
				{
					bestDist = dist;
					neighborName = otherPair.first;
				}
			}

			if (!neighborName.empty())
			{
				result.hitType = RayHitType::Panel;
				result.hitPanelName = neighborName;
			}

			return result;
		}

		void PanelMapBuilder::buildSplittersFromRays(
			const std::map<std::string, IFrameScreenData>& frameDataMap)
		{
			map_data_.splitterCandidates.clear();

			if (frameDataMap.empty())
			{
				return;
			}

			const int BORDER_OFFSET = 3;
			const int SPLITTER_THICKNESS = 10;

			int windowWidth = 0;
			int windowHeight = 0;

			for (const auto& p : frameDataMap)
			{
				if (p.second.windowWidth  > 0) windowWidth = p.second.windowWidth;
				if (p.second.windowHeight > 0) windowHeight = p.second.windowHeight;
				if (windowWidth > 0 && windowHeight > 0) break;
			}

			if (windowWidth <= 0 || windowHeight <= 0)
			{
				return;
			}

			const int borderLeft = BORDER_OFFSET;
			const int borderTop = BORDER_OFFSET;
			const int borderRight = windowWidth - BORDER_OFFSET;
			const int borderBottom = windowHeight - BORDER_OFFSET;

			std::map<std::string, SplitterCandidate> candidateMap;

			const RayDirection directions[4] =
			{
				RayDirection::Left,
				RayDirection::Right,
				RayDirection::Up,
				RayDirection::Down
			};

			for (const auto& panelPair : frameDataMap)
			{
				const std::string& panelName = panelPair.first;
				if (panelName == "viewport_panel") continue;
				if (map_data_.panels.find(panelName) == map_data_.panels.end()) continue;

				const IFrameScreenData& src = panelPair.second;
				const int px  = src.relativeX;
				const int py  = src.relativeY;
				const int pRight  = px + src.width;
				const int pBottom = py + src.height;

				for (RayDirection dir : directions)
				{
					RayCastResult hit = castRay(panelName, src, dir, frameDataMap,
						borderLeft, borderTop, borderRight, borderBottom);

					if (hit.hitType == RayHitType::AppBorder)
					{
						continue;
					}

					const bool isVertical = (dir == RayDirection::Left || dir == RayDirection::Right);

					// Compute the axis position used as dedup key.
					// Panel hit  -> midpoint between the two adjacent edges.
					// Nothing hit -> edge of the source panel.
					int axisPos = 0;
					if (hit.hitType == RayHitType::Panel)
					{
						auto nit = frameDataMap.find(hit.hitPanelName);
						if (nit != frameDataMap.end())
						{
							const IFrameScreenData& nb = nit->second;
							if (dir == RayDirection::Right) axisPos = (pRight  + nb.relativeX) / 2;
							else if (dir == RayDirection::Left) axisPos = (nb.relativeX + nb.width + px) / 2;
							else if (dir == RayDirection::Down) axisPos = (pBottom + nb.relativeY) / 2;
							else axisPos = (nb.relativeY + nb.height + py) / 2;
						}
					}
					else
					{
						if (dir == RayDirection::Right) axisPos = pRight;
						else if (dir == RayDirection::Left) axisPos = px - SPLITTER_THICKNESS;
						else if (dir == RayDirection::Down) axisPos = pBottom;
						else axisPos = py - SPLITTER_THICKNESS;
					}

					const std::string dedupKey = (isVertical ? "V|" : "H|") + std::to_string(axisPos);

					// Geometry of the splitter from this panel's perspective.
					const int sx = isVertical ? axisPos : px;
					const int sy = isVertical ? py : axisPos;
					const int sw = isVertical ? SPLITTER_THICKNESS : src.width;
					const int sh = isVertical ? src.height : SPLITTER_THICKNESS;

					auto it = candidateMap.find(dedupKey);
					if (it == candidateMap.end())
					{
						SplitterCandidate candidate;
						candidate.x = sx;
						candidate.y = sy;
						candidate.width = sw;
						candidate.height = sh;
						candidate.isVertical = isVertical;
						candidate.assignedPanels.push_back(panelName);
						if (hit.hitType == RayHitType::Panel && !hit.hitPanelName.empty())
						{
							candidate.assignedPanels.push_back(hit.hitPanelName);
						}
						candidateMap[dedupKey] = candidate;
					}
					else
					{
						SplitterCandidate& existing = it->second;

						if (isVertical)
						{
							const int minY = (std::min)(existing.y, sy);
							const int maxY = (std::max)(existing.y + existing.height, sy + sh);
							existing.y = minY;
							existing.height = maxY - minY;
						}
						else
						{
							const int minX = (std::min)(existing.x, sx);
							const int maxX = (std::max)(existing.x + existing.width, sx + sw);
							existing.x = minX;
							existing.width = maxX - minX;
						}

						auto addIfMissing = [&existing](const std::string& name)
						{
							if (!name.empty() &&
								std::find(existing.assignedPanels.begin(),
								          existing.assignedPanels.end(), name)
								== existing.assignedPanels.end())
							{
								existing.assignedPanels.push_back(name);
							}
						};

						addIfMissing(panelName);
						if (hit.hitType == RayHitType::Panel)
						{
							addIfMissing(hit.hitPanelName);
						}
					}
				}
			}

			for (auto& kv : candidateMap)
			{
				map_data_.splitterCandidates.push_back(kv.second);
			}
		}

	}
}