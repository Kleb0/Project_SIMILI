#pragma once

#include "../../ThreadSafeIFrameMap.hpp"
#include <string>
#include <map>

class FrameDataCatcher
{
public:
	FrameDataCatcher();
	~FrameDataCatcher() = default;

	static FrameDataCatcher* getInstance();
	static void setInstance(FrameDataCatcher* instance);

	bool receiveIFrameData(const std::string& jsonBody);
	std::map<std::string, IFrameData> getAllIFrames() const;

	ThreadSafeIFrameMap iframe_data_map_;

private:
	static FrameDataCatcher* s_instance_;
};
