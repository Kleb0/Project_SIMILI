#pragma once

#include <string>
#include <map>
#include <mutex>

struct IFrameData
{
	std::string name;
	int x;
	int y;
	int width;
	int height;
	int clientX;
	int clientY;
};

class IFrameSizeStocker
{
public:
	static IFrameSizeStocker& getInstance();

	void updateIFrameData(const std::string& name, int x, int y, int width, int height, int clientX = 0, int clientY = 0);
	
	bool getIFrameData(const std::string& name, IFrameData& outData) const;
	
	std::map<std::string, IFrameData> getAllIFrames() const;
	
	void clearAll();

private:
	IFrameSizeStocker() = default;
	~IFrameSizeStocker() = default;
	
	IFrameSizeStocker(const IFrameSizeStocker&) = delete;
	IFrameSizeStocker& operator=(const IFrameSizeStocker&) = delete;

	mutable std::mutex mutex_;
	std::map<std::string, IFrameData> iframeDataMap_;
};
