#include "IFrameSizeStocker.hpp"
#include <iostream>

IFrameSizeStocker& IFrameSizeStocker::getInstance()
{
	static IFrameSizeStocker instance;
	return instance;
}

void IFrameSizeStocker::updateIFrameData(const std::string& name, int x, int y, int width, int height, int clientX, int clientY)
{
	std::lock_guard<std::mutex> lock(mutex_);
	
	IFrameData data;
	data.name = name;
	data.x = x;
	data.y = y;
	data.width = width;
	data.height = height;
	data.clientX = clientX;
	data.clientY = clientY;
	
	iframeDataMap_[name] = data;
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
}
