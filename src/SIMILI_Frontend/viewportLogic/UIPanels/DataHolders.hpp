#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>

class DataHolders
{
public:
	DataHolders();
	~DataHolders() = default;

	static DataHolders* getInstance();
	static void setInstance(DataHolders* instance);

	bool receiveDataHolders(const std::string& jsonBody);
	int getCountForPanel(const std::string& panelName) const;

private:
	static DataHolders* s_instance_;

	mutable std::mutex mutex_;
	std::map<std::string, std::vector<std::string>> panel_fields_;
};
