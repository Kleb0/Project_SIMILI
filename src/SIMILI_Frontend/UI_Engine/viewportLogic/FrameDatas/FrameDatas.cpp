#include "FrameDatas.hpp"
#include <iostream>
#include <iomanip>

namespace SIMILI {
namespace Frontend {

FrameDatas::FrameDatas()
	: sizeStocker_(&IFrameSizeStocker::getInstance())
{
}

void FrameDatas::captureAllFrames(HWND cefWindowHandle)
{
	if (!cefWindowHandle)
	{
		std::cout << "[FrameDatas] Invalid HWND provided" << std::endl;
		return;
	}

	auto allFrames = sizeStocker_->getAllIFrames();
	
	if (allFrames.empty())
	{
		std::cout << "[FrameDatas] No frames available in IFrameSizeStocker" << std::endl;
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
	
	printAllFrameData();
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
	HDC hdc = GetDC(hwnd);
	if (!hdc)
	{
		return 1.0f;
	}
	
	int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
	ReleaseDC(hwnd, hdc);
	
	return static_cast<float>(dpiX) / 96.0f;
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

}
}
