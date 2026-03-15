#pragma once

#include <string>
#include <vector>
#include <cstdarg>
#include <cstdio>

namespace eb {

enum class LogLevel { Debug, Info, Warning, Error, Script };

struct LogEntry {
    LogLevel level;
    std::string message;
    float timestamp;
};

class DebugLog {
public:
    static DebugLog& instance() {
        static DebugLog log;
        return log;
    }

    void log(LogLevel level, const char* fmt, ...) {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        // Single-threaded game loop — no lock needed
        entries_.push_back({level, buf, time_});
        if ((int)entries_.size() > max_entries_) entries_.erase(entries_.begin());

        // Also print to stdout/stderr
        const char* prefix[] = {"[DBG]", "[INF]", "[WRN]", "[ERR]", "[SCR]"};
        std::fprintf(level == LogLevel::Error ? stderr : stdout,
                     "%s %s\n", prefix[(int)level], buf);
    }

    void debug(const char* fmt, ...)   { char buf[1024]; va_list a; va_start(a, fmt); vsnprintf(buf, sizeof(buf), fmt, a); va_end(a); log(LogLevel::Debug, "%s", buf); }
    void info(const char* fmt, ...)    { char buf[1024]; va_list a; va_start(a, fmt); vsnprintf(buf, sizeof(buf), fmt, a); va_end(a); log(LogLevel::Info, "%s", buf); }
    void warning(const char* fmt, ...) { char buf[1024]; va_list a; va_start(a, fmt); vsnprintf(buf, sizeof(buf), fmt, a); va_end(a); log(LogLevel::Warning, "%s", buf); }
    void error(const char* fmt, ...)   { char buf[1024]; va_list a; va_start(a, fmt); vsnprintf(buf, sizeof(buf), fmt, a); va_end(a); log(LogLevel::Error, "%s", buf); }
    void script(const char* fmt, ...)  { char buf[1024]; va_list a; va_start(a, fmt); vsnprintf(buf, sizeof(buf), fmt, a); va_end(a); log(LogLevel::Script, "%s", buf); }

    const std::vector<LogEntry>& entries() const { return entries_; }
    void clear() { entries_.clear(); }
    void set_time(float t) { time_ = t; }
    int max_entries() const { return max_entries_; }

    // Filter
    bool show_debug = true;
    bool show_info = true;
    bool show_warning = true;
    bool show_error = true;
    bool show_script = true;

private:
    DebugLog() = default;
    std::vector<LogEntry> entries_;
    // No mutex needed for single-threaded game loop
    float time_ = 0.0f;
    int max_entries_ = 500;
};

// Convenience macros
#define DLOG_DEBUG(...)   eb::DebugLog::instance().debug(__VA_ARGS__)
#define DLOG_INFO(...)    eb::DebugLog::instance().info(__VA_ARGS__)
#define DLOG_WARN(...)    eb::DebugLog::instance().warning(__VA_ARGS__)
#define DLOG_ERROR(...)   eb::DebugLog::instance().error(__VA_ARGS__)
#define DLOG_SCRIPT(...)  eb::DebugLog::instance().script(__VA_ARGS__)

} // namespace eb
