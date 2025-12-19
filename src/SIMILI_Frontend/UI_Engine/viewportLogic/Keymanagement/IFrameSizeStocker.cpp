#include "IFrameSizeStocker.hpp"
#include <iostream>

IFrameSizeStocker& IFrameSizeStocker::getInstance()
{
	static IFrameSizeStocker instance;
	return instance;
}

void IFrameSizeStocker::updateIFrameData(const std::string& name, int x, int y, int width, int height)
{
	std::lock_guard<std::mutex> lock(mutex_);
	
	IFrameData data;
	data.name = name;
	data.x = x;
	data.y = y;
	data.width = width;
	data.height = height;
	
	iframeDataMap_[name] = data;
	
	std::cout << "[IFrameSizeStocker] Updated: " << name 
			  << " - Position(" << x << "," << y << ") Size(" << width << "x" << height << ")" << std::endl;
}

bool IFrameSizeStocker::getIFrameData(const std::string& name, IFrameData& outData) const
{
	std::lock_guard<std::mutex> lock(mutex_);
	
	auto it = iframeDataMap_.find(name);
	if (it != iframeDataMap_.end())
	{
		outData = it->second;
		return true;
	}
	
	return false;
}

std::map<std::string, IFrameData> IFrameSizeStocker::getAllIFrames() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return iframeDataMap_;
}

void IFrameSizeStocker::clearAll()
{
	std::lock_guard<std::mutex> lock(mutex_);
	iframeDataMap_.clear();
	std::cout << "[IFrameSizeStocker] Cleared all iframe data" << std::endl;
}
