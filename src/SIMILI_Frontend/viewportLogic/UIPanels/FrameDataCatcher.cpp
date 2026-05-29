#include "FrameDataCatcher.hpp"
#include "json.hpp"
#include <iostream>

using json = nlohmann::json;

FrameDataCatcher* FrameDataCatcher::s_instance_ = nullptr;

FrameDataCatcher::FrameDataCatcher()
{
}

FrameDataCatcher* FrameDataCatcher::getInstance()
{
	return s_instance_;
}

void FrameDataCatcher::setInstance(FrameDataCatcher* instance)
{
	s_instance_ = instance;
}

bool FrameDataCatcher::receiveIFrameData(const std::string& jsonBody)
{
	json requestData = json::parse(jsonBody, nullptr, false);
	if (requestData.is_discarded())
	{
		return false;
	}

	if (!requestData.contains("iframes") || !requestData["iframes"].is_array())
	{
		return false;
	}

	for (const auto& iframe : requestData["iframes"])
	{
		if (!iframe.contains("name") || !iframe.contains("x") || !iframe.contains("y") ||
			!iframe.contains("width") || !iframe.contains("height"))
		{
			continue;
		}

		std::string name = iframe["name"];
		if (name == "viewport_panel")
		{
			continue;
		}

		IFrameData data;
		data.name = name;
		data.x = iframe["x"];
		data.y = iframe["y"];
		data.width = iframe["width"];
		data.height = iframe["height"];
		data.clientX = iframe.contains("clientX") ? iframe["clientX"].get<int>() : data.x;
		data.clientY = iframe.contains("clientY") ? iframe["clientY"].get<int>() : data.y;
		iframe_data_map_[name] = data;
	}

	return true;
}

std::map<std::string, IFrameData> FrameDataCatcher::getAllIFrames() const
{
	return iframe_data_map_.snapshot();
}
