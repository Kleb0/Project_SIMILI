#include "PanelMapBuilder.hpp"
#include "UIPanel.hpp"
#include "../../CEFDrawing/CEF_Drawer.hpp"
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

		void PanelMapBuilder::rebuildFromSource(
		const std::map<std::string, IFrameScreenData>& frameDataMap,
		int currentWindowWidth, int currentWindowHeight,
		std::map<std::string, PanelState>& panelStateMap,
		int& lastWindowWidth, int& lastWindowHeight)
		{
			std::cout << "[PanelMapBuilder::rebuildFromSource] Called with " << frameDataMap.size() << " panels for window " 
			          << currentWindowWidth << "x" << currentWindowHeight << std::endl;
			
			std::map<std::string, PanelState> previousPanelState = panelStateMap;
			int previousPanelCount = static_cast<int>(previousPanelState.size());
			
			panelStateMap.clear();

			int receivedWindowWidth = 0;
			int receivedWindowHeight = 0;
			int maxPanelRight = 0;
			int maxPanelBottom = 0;

			for (const auto& pair : frameDataMap)
			{
				PanelState state;
				state.frame = pair.second;
				state.client_offset_x = pair.second.clientX - pair.second.relativeX;
				state.client_offset_y = pair.second.clientY - pair.second.relativeY;
				panelStateMap[pair.first] = state;

				if (receivedWindowWidth == 0 && receivedWindowHeight == 0)
				{
					receivedWindowWidth = pair.second.windowWidth;
					receivedWindowHeight = pair.second.windowHeight;
				}

				if (pair.second.width > 0 && pair.second.height > 0)
				{
					int panelRight = pair.second.relativeX + pair.second.width;
					int panelBottom = pair.second.relativeY + pair.second.height;
					maxPanelRight = (std::max)(maxPanelRight, panelRight);
					maxPanelBottom = (std::max)(maxPanelBottom, panelBottom);
				}
			}

			bool needsImmediateScaling = false;
			int scalingSourceWidth = 0;
			int scalingSourceHeight = 0;

			if (receivedWindowWidth > 0 && receivedWindowHeight > 0 && maxPanelRight > 0 && maxPanelBottom > 0)
			{
				float panelUsageRatioX = static_cast<float>(maxPanelRight) / static_cast<float>(receivedWindowWidth);
				float panelUsageRatioY = static_cast<float>(maxPanelBottom) / static_cast<float>(receivedWindowHeight);
				
				std::cout << "[PanelMapBuilder::rebuildFromSource] Panel coverage: "
				          << maxPanelRight << "x" << maxPanelBottom 
				          << " vs window " << receivedWindowWidth << "x" << receivedWindowHeight
				          << " (ratios: " << panelUsageRatioX << " x " << panelUsageRatioY << ")" << std::endl;

				const float CRITICAL_MIN_THRESHOLD = 0.50f;
				const float CRITICAL_MAX_THRESHOLD = 2.00f;
				
				if (panelUsageRatioX < CRITICAL_MIN_THRESHOLD || panelUsageRatioY < CRITICAL_MIN_THRESHOLD)
				{
					std::cout << "[PanelMapBuilder::rebuildFromSource] CRITICAL: Panels cover only " 
					          << (panelUsageRatioX * 100.0f) << "% x " << (panelUsageRatioY * 100.0f) << "% of window" << std::endl;
					std::cout << "[PanelMapBuilder::rebuildFromSource] Coordinates too stale - rejecting and keeping previous state" << std::endl;
					
					if (!previousPanelState.empty() && lastWindowWidth > 0 && lastWindowHeight > 0)
					{
						std::cout << "[PanelMapBuilder::rebuildFromSource] Restoring previous valid state with " << previousPanelCount << " panels" << std::endl;
						panelStateMap = previousPanelState;
						
						if (currentWindowWidth > 0 && currentWindowHeight > 0 && 
						    (currentWindowWidth != lastWindowWidth || currentWindowHeight != lastWindowHeight))
						{
							std::cout << "[PanelMapBuilder::rebuildFromSource] Scaling restored panels from " 
							          << lastWindowWidth << "x" << lastWindowHeight 
							          << " to " << currentWindowWidth << "x" << currentWindowHeight << std::endl;
							
							const float scaleX = static_cast<float>(currentWindowWidth) / static_cast<float>(lastWindowWidth);
							const float scaleY = static_cast<float>(currentWindowHeight) / static_cast<float>(lastWindowHeight);
							
							for (auto& pair : panelStateMap)
							{
								IFrameScreenData& frame = pair.second.frame;
								frame.relativeX = static_cast<int>(std::lround(static_cast<float>(frame.relativeX) * scaleX));
								frame.relativeY = static_cast<int>(std::lround(static_cast<float>(frame.relativeY) * scaleY));
								frame.width = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(frame.width) * scaleX)));
								frame.height = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(frame.height) * scaleY)));
							
								if (frame.relativeX >= currentWindowWidth) frame.relativeX = (std::max)(0, currentWindowWidth - frame.width);
								if (frame.relativeY >= currentWindowHeight) frame.relativeY = (std::max)(0, currentWindowHeight - frame.height);
								if (frame.relativeX + frame.width > currentWindowWidth) frame.width = (std::max)(1, currentWindowWidth - frame.relativeX);
								if (frame.relativeY + frame.height > currentWindowHeight) frame.height = (std::max)(1, currentWindowHeight - frame.relativeY);
								
								frame.windowWidth = currentWindowWidth;
								frame.windowHeight = currentWindowHeight;
							}
							
							lastWindowWidth = currentWindowWidth;
							lastWindowHeight = currentWindowHeight;
						}
						
						std::cout << "[PanelMapBuilder::rebuildFromSource] Restoration complete - keeping " << panelStateMap.size() << " panels" << std::endl;
						return;
					}
					else
					{
						std::cout << "[PanelMapBuilder::rebuildFromSource] WARNING: No previous state available, accepting potentially stale coordinates" << std::endl;
					}
				}

				if (panelUsageRatioX > CRITICAL_MAX_THRESHOLD || panelUsageRatioY > CRITICAL_MAX_THRESHOLD)
				{
					std::cout << "[PanelMapBuilder::rebuildFromSource] CRITICAL: Panels exceed window bounds " 
					          << (panelUsageRatioX * 100.0f) << "% x " << (panelUsageRatioY * 100.0f) << "%" << std::endl;
					std::cout << "[PanelMapBuilder::rebuildFromSource] Coordinates too stale - rejecting and keeping previous state" << std::endl;
					
					if (!previousPanelState.empty() && lastWindowWidth > 0 && lastWindowHeight > 0)
					{
						std::cout << "[PanelMapBuilder::rebuildFromSource] Restoring previous valid state with " << previousPanelCount << " panels" << std::endl;
						panelStateMap = previousPanelState;
						
						if (currentWindowWidth > 0 && currentWindowHeight > 0 && 
						    (currentWindowWidth != lastWindowWidth || currentWindowHeight != lastWindowHeight))
						{
							std::cout << "[PanelMapBuilder::rebuildFromSource] Scaling restored panels from " 
							          << lastWindowWidth << "x" << lastWindowHeight 
							          << " to " << currentWindowWidth << "x" << currentWindowHeight << std::endl;
							
							const float scaleX = static_cast<float>(currentWindowWidth) / static_cast<float>(lastWindowWidth);
							const float scaleY = static_cast<float>(currentWindowHeight) / static_cast<float>(lastWindowHeight);
							
							for (auto& pair : panelStateMap)
							{
								IFrameScreenData& frame = pair.second.frame;
								frame.relativeX = static_cast<int>(std::lround(static_cast<float>(frame.relativeX) * scaleX));
								frame.relativeY = static_cast<int>(std::lround(static_cast<float>(frame.relativeY) * scaleY));
								frame.width = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(frame.width) * scaleX)));
								frame.height = (std::max)(1, static_cast<int>(std::lround(static_cast<float>(frame.height) * scaleY)));
							
								if (frame.relativeX >= currentWindowWidth) frame.relativeX = (std::max)(0, currentWindowWidth - frame.width);
								if (frame.relativeY >= currentWindowHeight) frame.relativeY = (std::max)(0, currentWindowHeight - frame.height);
								if (frame.relativeX + frame.width > currentWindowWidth) frame.width = (std::max)(1, currentWindowWidth - frame.relativeX);
								if (frame.relativeY + frame.height > currentWindowHeight) frame.height = (std::max)(1, currentWindowHeight - frame.relativeY);
							}
							
							lastWindowWidth = currentWindowWidth;
							lastWindowHeight = currentWindowHeight;
						}
						
						std::cout << "[PanelMapBuilder::rebuildFromSource] Restoration complete - keeping " << panelStateMap.size() << " panels" << std::endl;
						return;
					}
					else
					{
						std::cout << "[PanelMapBuilder::rebuildFromSource] WARNING: No previous state available, accepting potentially stale coordinates" << std::endl;
					}
				}

				const float MIN_RATIO = 0.70f;
				const float MAX_RATIO = 1.50f;
				
				if (panelUsageRatioX < MIN_RATIO || panelUsageRatioY < MIN_RATIO || 
				    panelUsageRatioX > MAX_RATIO || panelUsageRatioY > MAX_RATIO)
				{
					needsImmediateScaling = true;
					
					const float TYPICAL_PANEL_COVERAGE = 0.998f;
					int inferredSourceWidth = static_cast<int>(std::lround(static_cast<float>(maxPanelRight) / TYPICAL_PANEL_COVERAGE));
					int inferredSourceHeight = static_cast<int>(std::lround(static_cast<float>(maxPanelBottom) / TYPICAL_PANEL_COVERAGE));
					
					scalingSourceWidth = inferredSourceWidth;
					scalingSourceHeight = inferredSourceHeight;
					
					std::cout << "[PanelMapBuilder::rebuildFromSource] Detected mismatched panel coordinates!" << std::endl;
					std::cout << "  Panel coverage ratios: " << (panelUsageRatioX * 100.0f) << "% x " 
					          << (panelUsageRatioY * 100.0f) << "%" << std::endl;
					std::cout << "  Panel bounds: " << maxPanelRight << "x" << maxPanelBottom << std::endl;
					std::cout << "  Inferred source window size: " << scalingSourceWidth 
					          << "x" << scalingSourceHeight << std::endl;
					std::cout << "  JavaScript reported size: " << receivedWindowWidth << "x" << receivedWindowHeight << std::endl;
					std::cout << "  Actual window size: " << currentWindowWidth << "x" << currentWindowHeight << std::endl;
				}
			}

			if (currentWindowWidth > 0 && currentWindowHeight > 0)
			{
				if (needsImmediateScaling && scalingSourceWidth > 0 && scalingSourceHeight > 0)
				{
					std::cout << "[PanelMapBuilder::rebuildFromSource] Applying immediate scale from "
					          << scalingSourceWidth << "x" << scalingSourceHeight
					          << " to " << currentWindowWidth << "x" << currentWindowHeight << std::endl;
					
					const float scaleX = static_cast<float>(currentWindowWidth) / static_cast<float>(scalingSourceWidth);
					const float scaleY = static_cast<float>(currentWindowHeight) / static_cast<float>(scalingSourceHeight);
					
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
						
						if (frame.relativeX >= currentWindowWidth)
						{
							frame.relativeX = (std::max)(0, currentWindowWidth - frame.width);
							std::cout << "[PanelMapBuilder::rebuildFromSource] CLAMPED " << pair.first 
							          << " - was off-screen horizontally, moved to x=" << frame.relativeX << std::endl;
						}
						
						if (frame.relativeY >= currentWindowHeight)
						{
							frame.relativeY = (std::max)(0, currentWindowHeight - frame.height);
							std::cout << "[PanelMapBuilder::rebuildFromSource] CLAMPED " << pair.first 
							          << " - was off-screen vertically, moved to y=" << frame.relativeY << std::endl;
						}
						
						if (frame.relativeX + frame.width > currentWindowWidth)
						{
							frame.width = (std::max)(1, currentWindowWidth - frame.relativeX);
							std::cout << "[PanelMapBuilder::rebuildFromSource] CLAMPED " << pair.first 
							          << " width from " << oldW << " to " << frame.width << std::endl;
						}
						
						if (frame.relativeY + frame.height > currentWindowHeight)
						{
							frame.height = (std::max)(1, currentWindowHeight - frame.relativeY);
							std::cout << "[PanelMapBuilder::rebuildFromSource] CLAMPED " << pair.first 
							          << " height from " << oldH << " to " << frame.height << std::endl;
						}
						
						frame.windowWidth = currentWindowWidth;
						frame.windowHeight = currentWindowHeight;
						
						std::cout << "[PanelMapBuilder::rebuildFromSource] Scaled " << pair.first 
						          << ": (" << oldX << "," << oldY << " " << oldW << "x" << oldH << ")"
						          << " -> (" << frame.relativeX << "," << frame.relativeY << " " 
						          << frame.width << "x" << frame.height << ")" << std::endl;
					}
					
					lastWindowWidth = currentWindowWidth;
					lastWindowHeight = currentWindowHeight;
				}
				else
				{
					lastWindowWidth = currentWindowWidth;
					lastWindowHeight = currentWindowHeight;
					std::cout << "[PanelMapBuilder::rebuildFromSource] Updated last_window_ to: " 
					          << lastWindowWidth << "x" << lastWindowHeight << std::endl;
				}
			}

			createViewportPanel(panelStateMap, currentWindowWidth, currentWindowHeight);
			
			std::cout << "[PanelMapBuilder::rebuildFromSource] Completed - now tracking " << panelStateMap.size() 
			          << " panels (previous: " << previousPanelCount << ")" << std::endl;
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

			rebuildFromSource(frameDataMap, currentWindowWidth, currentWindowHeight, panelStateMap, lastWindowWidth, lastWindowHeight);

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

			buildSplitters(panelStateMap, splitterList);

			std::cout << "[PanelMapBuilder::syncSplittersAndPanes] Completed - " << panelStateMap.size() 
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

	void PanelMapBuilder::buildSplitters(
	const std::map<std::string, PanelState>& panelStateMap,
	std::vector<SplitterDefinition>& splitterList)
	{
		splitterList.clear();

		const int splitterThickness = 5;

		std::vector<const PanelState*> panels;
		for (const auto& pair : panelStateMap)
		{
			panels.push_back(&pair.second);
		}

		for (size_t i = 0; i < panels.size(); ++i)
		{
			const auto& panel1 = *panels[i];
			
			for (size_t j = i + 1; j < panels.size(); ++j)
			{
				const auto& panel2 = *panels[j];
				
				int panel1Right = panel1.frame.relativeX + panel1.frame.width;
				int panel2Right = panel2.frame.relativeX + panel2.frame.width;
				int panel1Bottom = panel1.frame.relativeY + panel1.frame.height;
				int panel2Bottom = panel2.frame.relativeY + panel2.frame.height;
				
				if (abs(panel1Right - panel2.frame.relativeX) < 20 || 
				    abs(panel2Right - panel1.frame.relativeX) < 20)
				{
					int overlapTop = (std::max)(panel1.frame.relativeY, panel2.frame.relativeY);
					int overlapBottom = (std::min)(panel1Bottom, panel2Bottom);
					
					if (overlapBottom > overlapTop + 50)
					{
						int splitterX = (panel1Right <= panel2.frame.relativeX) ? panel1Right : panel2Right;
						
						SplitterDefinition splitter;
						splitter.x = splitterX;
						splitter.y = overlapTop;
						splitter.width = splitterThickness;
						splitter.height = overlapBottom - overlapTop;
						splitter.isVertical = true;
						
						splitterList.push_back(splitter);
					}
				}
				
				if (abs(panel1Bottom - panel2.frame.relativeY) < 20 || 
				    abs(panel2Bottom - panel1.frame.relativeY) < 20)
				{
					int overlapLeft = (std::max)(panel1.frame.relativeX, panel2.frame.relativeX);
					int overlapRight = (std::min)(panel1Right, panel2Right);
					
					if (overlapRight > overlapLeft + 50)
					{
						int splitterY = (panel1Bottom <= panel2.frame.relativeY) ? panel1Bottom : panel2Bottom;
						
						SplitterDefinition splitter;
						splitter.x = overlapLeft;
						splitter.y = splitterY;
						splitter.width = overlapRight - overlapLeft;
						splitter.height = splitterThickness;
						splitter.isVertical = false;
						
						splitterList.push_back(splitter);
					}
				}
			}
		}

		std::cout << "[PanelMapBuilder::buildSplitters] Created " << splitterList.size() 
		          << " splitters (thickness=" << splitterThickness << "px)" << std::endl;
	}

		void PanelMapBuilder::drawInsideAppBorders(
		VkCommandBuffer commandBuffer,
		int drawableWidth, int drawableHeight,
		const std::map<std::string, IFrameScreenData>& panelFrameDataMap,
		bool skipTextureRebuild, VKContext* vkContext,
		VulkanPipeline* vulkanPipelines,
		VkRenderPass renderPass, CEF_Drawer* cefDrawer,
		std::map<std::string, std::unique_ptr<UIPanel>>& uiPanels,
		SDL_Window* window,
		int appBorderLeft, int appBorderTop,
		int appBorderWidth, int appBorderHeight)
		{
	
			if (!cefDrawer)
			{
				std::cout << "[PanelMapBuilder] drawInsideAppBorders: cefDrawer is null" << std::endl;
				return;
			}

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

	}
}
