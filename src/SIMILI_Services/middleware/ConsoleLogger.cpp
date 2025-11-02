#include "ConsoleLogger.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace SIMILI {
namespace Server {

// LogEntry implementation
LogEntry::LogEntry(const std::string& msg, const std::string& t) 
	: message(msg), type(t) 
{
}

// ConsoleLogger implementation

std::string ConsoleLogger::generateSessionId() 
{
	// Generate a random 8-character hexadecimal ID
	static std::random_device rd;
	static std::mt19937 gen(rd());
	static std::uniform_int_distribution<> dis(0, 0xFFFFFFFF);
	
	std::ostringstream oss;
	oss << std::hex << std::setw(8) << std::setfill('0') << dis(gen);
	return oss.str();
}

ConsoleLogger::ConsoleLogger() 
	: maxLogs_(1000), 
	  sessionInitialized_(false), 
	  applicationSessionId_(generateSessionId()),
	  consoleLoggerId_(generateSessionId())
{
	// Log singleton creation to verify it only happens once
	std::cout << "[ConsoleLogger] ConsoleLogger instance ID: " << consoleLoggerId_ << std::endl;
	std::cout << "[ConsoleLogger] Application Session ID: " << applicationSessionId_ << std::endl;
}

ConsoleLogger& ConsoleLogger::getInstance() 
{
	// Thread-safe singleton initialization (C++11 magic statics)
	static ConsoleLogger instance;
	return instance;
}

void ConsoleLogger::addLog(const std::string& message, const std::string& type) 
{
	std::lock_guard<std::mutex> lock(mutex_);
	logs_.emplace_back(message, type);
	if (logs_.size() > maxLogs_) {
		logs_.erase(logs_.begin());
	}
}

std::vector<LogEntry> ConsoleLogger::getLogs(bool clearAfter) 
{
	std::lock_guard<std::mutex> lock(mutex_);
	std::vector<LogEntry> result = logs_;
	if (clearAfter) {
		logs_.clear();
	}
	return result;
}

void ConsoleLogger::clear() 
{
	std::lock_guard<std::mutex> lock(mutex_);
	logs_.clear();
}

bool ConsoleLogger::initializeSession() 
{
	// Atomic compare-and-swap: only ONE thread will succeed
	bool expected = false;
	if (sessionInitialized_.compare_exchange_strong(expected, true)) {
		// This thread won the race - it initializes
		return true;
	}
	// Another thread already initialized
	return false;
}

bool ConsoleLogger::isSessionInitialized() const 
{
	return sessionInitialized_.load();
}

const std::string& ConsoleLogger::getApplicationSessionId() const 
{
	return applicationSessionId_;
}

const std::string& ConsoleLogger::getConsoleLoggerId() const 
{
	return consoleLoggerId_;
}

} // namespace Server
} // namespace SIMILI
