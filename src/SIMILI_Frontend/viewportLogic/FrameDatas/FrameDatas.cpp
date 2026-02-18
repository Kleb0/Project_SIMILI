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

				void FrameDatas::captureAllFrames(HWND cefWindowHandle)
				{
					if (!cefWindowHandle)
					{
						std::cout << "[FrameDatas] Invalid HWND provided" << std::endl;
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
				
					captureWindowData(cefWindowHandle, screenData);
					
					RECT windowRect;
					GetWindowRect(cefWindowHandle, &windowRect);
					
					screenData.screenX = windowRect.left + frameData.clientX;
					screenData.screenY = windowRect.top + frameData.clientY;
					
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

			float FrameDatas::getDPIScale(HWND hwnd) const
			{
				if (!hwnd)
				{
					return 1.0f;
				}
				
				HMONITOR hMonitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
				if (!hMonitor)
				{
					return 1.0f;
				}
				
				typedef HRESULT(WINAPI* GetDpiForMonitorFunc)(HMONITOR, int, UINT*, UINT*);
				HMODULE shcore = LoadLibraryA("Shcore.dll");
				if (!shcore)
				{
					HDC hdc = GetDC(hwnd);
					if (!hdc)
					{
						return 1.0f;
					}
					int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
					ReleaseDC(hwnd, hdc);
					return static_cast<float>(dpiX) / 96.0f;
				}
				
				GetDpiForMonitorFunc getDpiForMonitor = (GetDpiForMonitorFunc)GetProcAddress(shcore, "GetDpiForMonitor");
				if (!getDpiForMonitor)
				{
					FreeLibrary(shcore);
					HDC hdc = GetDC(hwnd);
					if (!hdc)
					{
						return 1.0f;
					}
					int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
					ReleaseDC(hwnd, hdc);
					return static_cast<float>(dpiX) / 96.0f;
				}
				
				UINT dpiX = 96;
				UINT dpiY = 96;
				HRESULT hr = getDpiForMonitor(hMonitor, 0, &dpiX, &dpiY);
				
				FreeLibrary(shcore);
				
				if (SUCCEEDED(hr))
				{
					return static_cast<float>(dpiX) / 96.0f;
				}
				
				HDC hdc = GetDC(hwnd);
				if (!hdc)
				{
					return 1.0f;
				}
				int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
				ReleaseDC(hwnd, hdc);
				return static_cast<float>(dpi) / 96.0f;
			}

			void FrameDatas::captureWindowData(HWND hwnd, IFrameScreenData& data)
			{
				RECT windowRect;
				GetWindowRect(hwnd, &windowRect);
				
				data.windowX = windowRect.left;
				data.windowY = windowRect.top;
				data.windowWidth = windowRect.right - windowRect.left;
				data.windowHeight = windowRect.bottom - windowRect.top;
				
				data.dpiScale = getDPIScale(hwnd);
				
				data.screenWidth = GetSystemMetrics(SM_CXSCREEN);
				data.screenHeight = GetSystemMetrics(SM_CYSCREEN);
			}

		void FrameDatas::updateFrameData(const std::string& name, int relativeX, int relativeY, int width, int height, int clientX, int clientY, HWND cefWindowHandle)
		{
			if (!cefWindowHandle)
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
				
				captureWindowData(cefWindowHandle, data);
				
				RECT windowRect;
				GetWindowRect(cefWindowHandle, &windowRect);
				
				data.screenX = windowRect.left + clientX;
				data.screenY = windowRect.top + clientY;
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
				
				captureWindowData(cefWindowHandle, newData);
				
				RECT windowRect;
				GetWindowRect(cefWindowHandle, &windowRect);
				
				newData.screenX = windowRect.left + clientX;
				newData.screenY = windowRect.top + clientY;
				
				frameDataMap_[name] = newData;
			}
		}
	}	
} 