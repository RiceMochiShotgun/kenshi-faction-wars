#include "fw/Log.h"

#include <sstream>

namespace fw {

void Log::add(LogLevel level, const std::string& origin, const std::string& message)
{
	LogEntry e;
	e.level = level;
	e.origin = origin;
	e.message = message;
	entries_.push_back(e);
	if (limit_ && entries_.size() > limit_)
		entries_.erase(entries_.begin(), entries_.begin() + (entries_.size() - limit_));
}

size_t Log::count(LogLevel atLeast) const
{
	size_t n = 0;
	for (size_t i = 0; i < entries_.size(); ++i)
		if (entries_[i].level >= atLeast) ++n;
	return n;
}

const char* Log::levelName(LogLevel l)
{
	switch (l)
	{
	case LOG_DEBUG: return "debug";
	case LOG_INFO: return "info";
	case LOG_WARNING: return "warning";
	default: return "error";
	}
}

std::string Log::format(LogLevel atLeast) const
{
	std::ostringstream ss;
	for (size_t i = 0; i < entries_.size(); ++i)
	{
		const LogEntry& e = entries_[i];
		if (e.level < atLeast) continue;
		ss << "[" << levelName(e.level) << "] " << e.origin << ": " << e.message << "\n";
	}
	return ss.str();
}

} // namespace fw
