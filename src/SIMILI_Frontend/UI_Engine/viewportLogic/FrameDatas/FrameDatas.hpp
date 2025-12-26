#pragma once

#include "../Keymanagement/IFrameSizeStocker.hpp"
#include <windows.h>
#include <string>
#include <map>

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
	FrameDatas();
	~FrameDatas() = default;

	void captureAllFrames(HWND cefWindowHandle);
	
	const std::map<std::string, IFrameScreenData>& getFrameData() const { return frameDataMap_; }
	
	bool getFrameData(const std::string& name, IFrameScreenData& outData) const;
	
	void printAllFrameData() const;

private:
	float getDPIScale(HWND hwnd) const;
	
	void captureWindowData(HWND hwnd, IFrameScreenData& data);
	
	std::map<std::string, IFrameScreenData> frameDataMap_;
	IFrameSizeStocker* sizeStocker_;
};

}
}
