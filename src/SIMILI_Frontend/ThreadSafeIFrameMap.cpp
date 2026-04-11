#include "ThreadSafeIFrameMap.hpp"
#include <mutex>

ThreadSafeIFrameMap::WriteProxy::WriteProxy(ThreadSafeIFrameMap* owner, const std::string& key)
	: owner_(owner)
	, key_(key)
{
}

ThreadSafeIFrameMap::WriteProxy& ThreadSafeIFrameMap::WriteProxy::operator=(const IFrameData& value)
{
	if (owner_)
	{
		std::lock_guard<std::mutex> lock(owner_->mutex_);
		owner_->data_[key_] = value;
	}
	return *this;
}

ThreadSafeIFrameMap::WriteProxy ThreadSafeIFrameMap::operator[](const std::string& key)
{
	return WriteProxy(this, key);
}

std::map<std::string, IFrameData> ThreadSafeIFrameMap::snapshot() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return data_;
}

void ThreadSafeIFrameMap::clear()
{
	std::lock_guard<std::mutex> lock(mutex_);
	data_.clear();
}

bool ThreadSafeIFrameMap::empty() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return data_.empty();
}

std::size_t ThreadSafeIFrameMap::size() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return data_.size();
}
