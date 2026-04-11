#pragma once

#include <map>
#include <string>
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

class ThreadSafeIFrameMap
{
public:
	class WriteProxy
	{
	public:
		WriteProxy(ThreadSafeIFrameMap* owner, const std::string& key);
		WriteProxy& operator=(const IFrameData& value);

	private:
		ThreadSafeIFrameMap* owner_;
		std::string key_;
	};

	WriteProxy operator[](const std::string& key);
	std::map<std::string, IFrameData> snapshot() const;
	void clear();
	bool empty() const;
	std::size_t size() const;

private:
	friend class WriteProxy;
	mutable std::mutex mutex_;
	std::map<std::string, IFrameData> data_;
};
