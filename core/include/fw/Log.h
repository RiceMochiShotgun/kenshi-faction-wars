// Collected messages for FactionWars.log: config problems, simulation events.
#ifndef FW_LOG_H
#define FW_LOG_H

#include <string>
#include <vector>

namespace fw {

enum LogLevel { LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR };

struct LogEntry
{
	LogLevel level;
	std::string origin;   // file name or subsystem
	std::string message;
};

class Log
{
public:
	void add(LogLevel level, const std::string& origin, const std::string& message);
	void debug(const std::string& origin, const std::string& m) { add(LOG_DEBUG, origin, m); }
	void info(const std::string& origin, const std::string& m) { add(LOG_INFO, origin, m); }
	void warn(const std::string& origin, const std::string& m) { add(LOG_WARNING, origin, m); }
	void error(const std::string& origin, const std::string& m) { add(LOG_ERROR, origin, m); }

	const std::vector<LogEntry>& entries() const { return entries_; }
	size_t count(LogLevel atLeast) const;
	void clear() { entries_.clear(); }
	// Drop oldest entries beyond this many (0 = unlimited).
	void setLimit(size_t n) { limit_ = n; }

	std::string format(LogLevel atLeast = LOG_INFO) const;
	static const char* levelName(LogLevel l);

	Log() : limit_(0) {}

private:
	std::vector<LogEntry> entries_;
	size_t limit_;
};

} // namespace fw

#endif
