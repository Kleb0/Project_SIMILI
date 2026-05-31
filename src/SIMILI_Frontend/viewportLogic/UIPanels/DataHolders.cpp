#include "DataHolders.hpp"
#include "json.hpp"
#include <iostream>
#include <mutex>

using json = nlohmann::json;

DataHolders* DataHolders::s_instance_ = nullptr;

DataHolders::DataHolders()
{
}

DataHolders* DataHolders::getInstance()
{
	return s_instance_;
}

void DataHolders::setInstance(DataHolders* instance)
{
	s_instance_ = instance;
}

bool DataHolders::receiveDataHolders(const std::string& jsonBody)
{
	json requestData = json::parse(jsonBody, nullptr, false);
	if (requestData.is_discarded())
	{
		return false;
	}

	if (!requestData.contains("panel") || !requestData.contains("dataHolders") || !requestData["dataHolders"].is_array())
	{
		return false;
	}

	std::string panelName = requestData["panel"];
	std::vector<std::string> fields;

	for (const auto& holder : requestData["dataHolders"])
	{
		if (holder.contains("field"))
		{
			fields.push_back(holder["field"].get<std::string>());
		}
	}

	{
		std::lock_guard<std::mutex> lock(mutex_);
		panel_fields_[panelName] = fields;
	}

	std::cout << "[DataHolders] " << panelName << " -> Contains " << fields.size() << " DataHolder(s)" << std::endl;

	return true;
}

int DataHolders::getCountForPanel(const std::string& panelName) const
{
	std::lock_guard<std::mutex> lock(mutex_);
	auto it = panel_fields_.find(panelName);
	if (it != panel_fields_.end())
	{
		return static_cast<int>(it->second.size());
	}
	return 0;
}
