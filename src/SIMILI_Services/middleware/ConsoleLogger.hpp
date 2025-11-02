#pragma once

#include <vector>
#include <string>
#include <mutex>
#include <atomic>

namespace SIMILI {
namespace Server {

// Log entry structure
struct LogEntry {
	std::string message;
	std::string type;
	LogEntry(const std::string& msg, const std::string& t);
};

// Singleton logger for console
class ConsoleLogger 
{
	std::vector<LogEntry> logs_;
	std::mutex mutex_;
	size_t maxLogs_;
	std::atomic<bool> sessionInitialized_; // Atomic for thread-safe check
	std::string applicationSessionId_; // Unique application session ID
	std::string consoleLoggerId_; // Unique ConsoleLogger instance ID

	// Private constructor for singleton
	ConsoleLogger();
	
	// Prevent copying
	ConsoleLogger(const ConsoleLogger&) = delete;
	ConsoleLogger& operator=(const ConsoleLogger&) = delete;
	
	// Generate a random session ID
	static std::string generateSessionId();

public:
	static ConsoleLogger& getInstance();

	void addLog(const std::string& message, const std::string& type = "server");
	std::vector<LogEntry> getLogs(bool clearAfter = true);
	void clear();

	// Initialize session (called only once on first HTTP request)
	bool initializeSession();
	bool isSessionInitialized() const;
	
	// Get the unique application session ID
	const std::string& getApplicationSessionId() const;
	
	// Get the unique ConsoleLogger instance ID
	const std::string& getConsoleLoggerId() const;
};

} // namespace Server
} // namespace SIMILI
