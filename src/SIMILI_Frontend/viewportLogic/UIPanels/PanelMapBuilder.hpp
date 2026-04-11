#pragma once

#include "../FrameDatas/FrameDatas.hpp"
#include <SDL3/SDL.h>
#include <map>
#include <string>

class VKContext;

namespace SIMILI {
	namespace Frontend {
		
		struct PanelState
		{
			IFrameScreenData frame;
			int client_offset_x;
			int client_offset_y;
		};

		class PanelMapBuilder
		{
		public:
			PanelMapBuilder();
			~PanelMapBuilder();

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

		private:
			void createViewportPanel(
				std::map<std::string, PanelState>& panelStateMap,
				int currentWindowWidth,
				int currentWindowHeight
			);
		};
	}
}
