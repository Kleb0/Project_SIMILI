#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <map>

class SimpleWindowDelegate;

namespace SIMILI {
	namespace Frontend {

		struct IFrameScreenData
		{
			std::string name;
			
			int relativeX;
			int relativeY;
			int width;
			int height;
			int clientX;
			int clientY;
			
			int screenX;
			int screenY;
			
			int windowX;
			int windowY;
			int windowWidth;
			int windowHeight;
			
			float dpiScale;
			int screenWidth;
			int screenHeight;
		};

		class FrameDatas
		{
		public:
			FrameDatas(SimpleWindowDelegate* windowDelegate);
			~FrameDatas() = default;

			void captureAllFrames(SDL_Window* sdlWindow);
			
			const std::map<std::string, IFrameScreenData>& getFrameData() const { return frameDataMap_; }
			
			bool getFrameData(const std::string& name, IFrameScreenData& outData) const;
			
			void printAllFrameData() const;
			
			void updateFrameData(const std::string& name, int relativeX, int relativeY, int width, int height, int clientX, int clientY, SDL_Window* sdlWindow);

		private:
			float getDPIScale(SDL_Window* sdlWindow) const;
			
			void captureWindowData(SDL_Window* sdlWindow, IFrameScreenData& data);
			
			std::map<std::string, IFrameScreenData> frameDataMap_;
			SimpleWindowDelegate* windowDelegate_;
		};
	}
}
