#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <map>
#include <vector>

class FrameDataCatcher;

namespace SIMILI {
	namespace Frontend {

		// Lightweight value type used as the public exchange format.
		// Internal storage inside IFrameDatas uses parallel arrays (SoA).
		class IFrameDatas
		{
			public:
				IFrameDatas();
				~IFrameDatas() = default;

				void catchFrameData(SDL_Window* sdlWindow);

				// Returns a rebuilt map for callers that need key-based lookup.

				// Fills outData for the named frame; returns false if not found.
				void printAllFrameData() const;

				void setDatas(const std::string& name, int width, int height, int screenX, int screenY, int windowWidth, int windowHeight);
				void getDatas(const std::string& name, int& width, int& height, int& screenX, int& screenY, int& windowWidth, int& windowHeight);

			private:

				std::string name_;

				int width_;
				int height_;

				int screenX_;
				int screenY_;

				int windowWidth_;
				int windowHeight_;

		};
	}
}
