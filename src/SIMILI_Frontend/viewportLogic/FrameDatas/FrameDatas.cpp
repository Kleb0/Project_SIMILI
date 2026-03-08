#include "FrameDatas.hpp"
#include "../../simple_window_delegate.hpp"
#include <iostream>
#include <iomanip>

namespace SIMILI {
	namespace Frontend {

			FrameDatas::FrameDatas(SimpleWindowDelegate* windowDelegate)
				: windowDelegate_(windowDelegate)
			{
			}

void FrameDatas::captureAllFrames(SDL_Window* sdlWindow)
			{
					if (!sdlWindow)
					{
						std::cout << "[FrameDatas] Invalid SDL_Window provided" << std::endl;
						return;
					}
				
				if (!windowDelegate_)
				{
					std::cout << "[FrameDatas] No window delegate available" << std::endl;
					return;
				}

				auto allFrames = windowDelegate_->getAllIFrames();
				
				if (allFrames.empty())
				{
					std::cout << "[FrameDatas] No frames available in SimpleWindowDelegate" << std::endl;
					return;
				}

				frameDataMap_.clear();

				for (const auto& pair : allFrames)
				{
					const IFrameData& frameData = pair.second;
					IFrameScreenData screenData;
					
					screenData.name = frameData.name;
					screenData.relativeX = frameData.x;
					screenData.relativeY = frameData.y;
					screenData.width = frameData.width;
					screenData.height = frameData.height;
					screenData.clientX = frameData.clientX;
					screenData.clientY = frameData.clientY;
				
					captureWindowData(sdlWindow, screenData);
					
					int wx, wy;
					SDL_GetWindowPosition(sdlWindow, &wx, &wy);
					
					screenData.screenX = wx + frameData.clientX;
					screenData.screenY = wy + frameData.clientY;
					
					frameDataMap_[frameData.name] = screenData;
				}
					
				// printAllFrameData();
			}

			bool FrameDatas::getFrameData(const std::string& name, IFrameScreenData& outData) const
			{
				auto it = frameDataMap_.find(name);
				if (it != frameDataMap_.end())
				{
					outData = it->second;
					return true;
				}
				return false;
			}

			void FrameDatas::printAllFrameData() const
			{
				std::cout << "\n========== FrameDatas - Captured Frame Data ==========" << std::endl;
				std::cout << std::fixed << std::setprecision(2);
				
				for (const auto& pair : frameDataMap_)
				{
					const IFrameScreenData& data = pair.second;
					
					std::cout << "\n--- Frame: " << data.name << " ---" << std::endl;
					std::cout << "Relative Position (CEF): X=" << data.relativeX << " Y=" << data.relativeY << std::endl;
					std::cout << "Client Position (CEF): X=" << data.clientX << " Y=" << data.clientY << std::endl;
					std::cout << "Size: W=" << data.width << " H=" << data.height << std::endl;
					std::cout << "Screen Position: X=" << data.screenX << " Y=" << data.screenY << std::endl;
					std::cout << "Window Position: X=" << data.windowX << " Y=" << data.windowY << std::endl;
					std::cout << "Window Size: W=" << data.windowWidth << " H=" << data.windowHeight << std::endl;
					std::cout << "DPI Scale: " << data.dpiScale << std::endl;
					std::cout << "Screen Resolution: " << data.screenWidth << "x" << data.screenHeight << std::endl;
				}
				
				std::cout << "\n========================================================\n" << std::endl;
			}

			float FrameDatas::getDPIScale(SDL_Window* sdlWindow) const
			{
				if (!sdlWindow)
				{
					return 1.0f;
				}
				
				int displayIndex = SDL_GetDisplayForWindow(sdlWindow);
				if (displayIndex < 0)
				{
					return 1.0f;
				}
				
				float ddpi = SDL_GetDisplayContentScale(displayIndex);
				if (ddpi > 0.0f)
				{
					return ddpi;
				}
				
				return 1.0f;
			}

			void FrameDatas::captureWindowData(SDL_Window* sdlWindow, IFrameScreenData& data)
			{
				int wx, wy, ww, wh;
				SDL_GetWindowPosition(sdlWindow, &wx, &wy);
				SDL_GetWindowSize(sdlWindow, &ww, &wh);
				
				data.windowX = wx;
				data.windowY = wy;
				data.windowWidth = ww;
				data.windowHeight = wh;
				
				data.dpiScale = getDPIScale(sdlWindow);
				
				int displayIndex = SDL_GetDisplayForWindow(sdlWindow);
				if (displayIndex >= 0)
				{
					const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displayIndex);
					if (mode)
					{
						data.screenWidth = mode->w;
						data.screenHeight = mode->h;
					}
					else
					{
						data.screenWidth = 1920;
						data.screenHeight = 1080;
					}
				}
				else
				{
					data.screenWidth = 1920;
					data.screenHeight = 1080;
				}
			}

		void FrameDatas::updateFrameData(const std::string& name, int relativeX, int relativeY, int width, int height, int clientX, int clientY, SDL_Window* sdlWindow)
		{
			if (!sdlWindow)
			{
				return;
			}

			auto it = frameDataMap_.find(name);
			if (it != frameDataMap_.end())
			{
				IFrameScreenData& data = it->second;
				
				data.relativeX = relativeX;
				data.relativeY = relativeY;
				data.width = width;
				data.height = height;
				data.clientX = clientX;
				data.clientY = clientY;
				
				captureWindowData(sdlWindow, data);
				
				int wx, wy;
				SDL_GetWindowPosition(sdlWindow, &wx, &wy);
				
				data.screenX = wx + static_cast<int>(clientX * data.dpiScale);
				data.screenY = wy + static_cast<int>(clientY * data.dpiScale);
			}
			else
			{
				IFrameScreenData newData;
				newData.name = name;
				newData.relativeX = relativeX;
				newData.relativeY = relativeY;
				newData.width = width;
				newData.height = height;
				newData.clientX = clientX;
				newData.clientY = clientY;
				
				captureWindowData(sdlWindow, newData);
				
				int wx, wy;
				SDL_GetWindowPosition(sdlWindow, &wx, &wy);
				
				newData.screenX = wx + static_cast<int>(clientX * newData.dpiScale);
				newData.screenY = wy + static_cast<int>(clientY * newData.dpiScale);
				
				frameDataMap_[name] = newData;
			}
		}
	}	
} 